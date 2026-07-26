#!/usr/bin/env python3
"""Route le PCB via freerouting, puis réimporte le résultat — P7 suite.

Troisième étage du pipeline PCB, après generate_pcb.py :

    1. exporte le PCB placé en Specctra DSN ;
    2. lance freerouting (autorouteur Java) en mode headless ;
    3. réimporte la session routée (.ses) dans le .kicad_pcb ;
    4. re-remplit les plans de masse.

    python3 route_pcb.py                    # cherche freerouting.jar
    python3 route_pcb.py --jar chemin.jar   # chemin explicite

Le routage automatique passe le DRC mais sa QUALITÉ n'est pas garantie sur les
points sensibles (plan de masse, antenne, paire USB). Une relecture humaine dans
KiCad reste nécessaire avant fabrication — voir docs/pcb.md.

freerouting.jar n'est PAS versionné (55 Mo). Le télécharger depuis
https://github.com/freerouting/freerouting/releases et le passer via --jar, ou
le placer dans le dossier parent du dépôt.
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path

import pcbnew

HERE = Path(__file__).resolve().parent
PCB = HERE / "vario.kicad_pcb"
DSN = Path("/tmp/vario.dsn")
SES = Path("/tmp/vario.ses")


def find_jar(explicit):
    if explicit:
        return Path(explicit)
    candidates = [
        HERE / "freerouting.jar",
        HERE.parent.parent.parent / "freerouting.jar",
        Path(os.environ.get("FREEROUTING_JAR", "")),
    ]
    for c in candidates:
        if c and c.exists():
            return c
    return None


def refill_zones(board):
    """Re-remplit les plans de masse après réimport, avec un thermal relief et
    une clearance permettant à la zone d'atteindre le plus de pads GND possible.
    Quelques pads enclavés peuvent rester non connectés — finition à la main
    dans KiCad (voir docs/pcb.md)."""
    for z in board.Zones():
        z.SetLocalClearance(pcbnew.FromMM(0.2))
        z.SetThermalReliefGap(pcbnew.FromMM(0.25))
        z.SetThermalReliefSpokeWidth(pcbnew.FromMM(0.35))
        z.SetMinThickness(pcbnew.FromMM(0.2))
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.BuildConnectivity()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jar", help="chemin de freerouting.jar")
    ap.add_argument("--passes", type=int, default=100,
                    help="nombre de passes de l'autorouteur")
    args = ap.parse_args()

    if not PCB.exists():
        raise SystemExit(f"PCB absent : {PCB} (lancer generate_pcb.py)")
    jar = find_jar(args.jar)
    if jar is None:
        raise SystemExit(
            "freerouting.jar introuvable. Le télécharger depuis "
            "https://github.com/freerouting/freerouting/releases puis :\n"
            "  python3 route_pcb.py --jar /chemin/freerouting.jar"
        )

    board = pcbnew.LoadBoard(str(PCB))

    # 1. Export Specctra DSN.
    if not pcbnew.ExportSpecctraDSN(board, str(DSN)):
        raise SystemExit("échec de l'export DSN")
    print(f"DSN exporté : {DSN}")

    # 2. freerouting headless.
    cmd = [
        "java", "-Djava.awt.headless=true", "-jar", str(jar),
        "-de", str(DSN), "-do", str(SES), "-mp", str(args.passes),
    ]
    print("freerouting :", " ".join(cmd))
    res = subprocess.run(cmd, capture_output=True, text=True, timeout=900)
    sys.stdout.write(res.stdout[-2000:])
    if not SES.exists():
        sys.stderr.write(res.stderr[-2000:])
        raise SystemExit("freerouting n'a pas produit de .ses")
    print(f"session routée : {SES}")

    # 3. Réimport.
    board = pcbnew.LoadBoard(str(PCB))
    if not pcbnew.ImportSpecctraSES(board, str(SES)):
        raise SystemExit("échec du réimport SES")

    # 4. Re-remplir les zones et sauver.
    refill_zones(board)
    board.Save(str(PCB))
    print(f"PCB routé sauvegardé : {PCB}")


if __name__ == "__main__":
    main()
