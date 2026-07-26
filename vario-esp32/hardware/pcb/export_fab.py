#!/usr/bin/env python3
"""Exporte les fichiers de fabrication JLCPCB — dernière étape du pipeline PCB.

Produit, dans build/fab/ :
  - les Gerbers (2 couches) + le fichier de perçage ;
  - le fichier de position des composants (CPL) au format JLCPCB ;
  - la nomenclature (BOM) au format JLCPCB, avec références LCSC ;
  - une archive .zip prête à téléverser.

    python3 export_fab.py

Nécessite kicad-cli (KiCad 9). Les fichiers engendrés vont dans build/, ignoré
par git. Le PCB n'a JAMAIS été fabriqué ni testé — voir docs/pcb.md.
"""

import csv
import subprocess
import sys
import zipfile
from pathlib import Path

import pcbnew

HERE = Path(__file__).resolve().parent
PCB = HERE / "vario.kicad_pcb"
SCH = HERE / "vario.kicad_sch"
OUT = HERE.parent.parent / "build" / "fab"

# Couches Gerber pour une carte 2 couches JLCPCB.
LAYERS = "F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts"


def run(cmd):
    print(" ", " ".join(str(c) for c in cmd))
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit(f"échec : {cmd[:3]}")


def export_gerbers():
    run(["kicad-cli", "pcb", "export", "gerbers", "-o", str(OUT) + "/",
         "--layers", LAYERS, "--no-protel-ext", str(PCB)])
    run(["kicad-cli", "pcb", "export", "drill", "-o", str(OUT) + "/",
         "--format", "excellon", "--drill-origin", "absolute",
         "--excellon-units", "mm", str(PCB)])


def export_cpl():
    """Fichier de position (CPL) au format JLCPCB : Designator, Mid X, Mid Y,
    Layer, Rotation."""
    pos = OUT / "vario-pos.csv"
    run(["kicad-cli", "pcb", "export", "pos", "-o", str(pos),
         "--format", "csv", "--units", "mm", "--use-drill-file-origin",
         "--side", "both", str(PCB)])
    # Réécrit avec les en-têtes JLCPCB.
    jlc = OUT / "vario-cpl-jlcpcb.csv"
    with open(pos) as f:
        rows = list(csv.DictReader(f))
    with open(jlc, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        for r in rows:
            layer = "top" if r.get("Side", "top").lower().startswith("t") else "bottom"
            w.writerow([r["Ref"], r["PosX"], r["PosY"], layer, r["Rot"]])
    print(f"  CPL JLCPCB : {jlc}")


def export_bom():
    """BOM JLCPCB : Comment, Designator, Footprint, LCSC Part #.

    Regroupe par (valeur, empreinte, LCSC) et concatène les repères."""
    # Le champ LCSC n'est pas transporté par la netlist : on le relit depuis la
    # table de référence du générateur de schéma (source de vérité unique).
    from generate_schematic import LCSC

    board = pcbnew.LoadBoard(str(PCB))
    groups = {}
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        val = fp.GetValue()
        fpid = fp.GetFPIDAsString()
        lcsc = LCSC.get(ref, "")
        key = (val, fpid, lcsc)
        groups.setdefault(key, []).append(ref)

    bom = OUT / "vario-bom-jlcpcb.csv"
    with open(bom, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
        for (val, fpid, lcsc), refs in sorted(groups.items()):
            refs_sorted = ",".join(sorted(refs, key=_natural))
            w.writerow([val, refs_sorted, fpid.split(":")[-1], lcsc])
    print(f"  BOM JLCPCB : {bom}")


def _natural(ref):
    import re
    m = re.match(r"([A-Za-z]+)(\d+)", ref)
    return (m.group(1), int(m.group(2))) if m else (ref, 0)


def make_zip():
    z = OUT.parent / "vario-gerbers.zip"
    with zipfile.ZipFile(z, "w", zipfile.ZIP_DEFLATED) as zf:
        for p in OUT.iterdir():
            if p.suffix.lower() in (".gbr", ".drl", ".gm1") or "gbr" in p.suffix.lower():
                zf.write(p, p.name)
    print(f"  archive Gerbers : {z}")


def main():
    if not PCB.exists():
        raise SystemExit(f"PCB absent : {PCB}")
    OUT.mkdir(parents=True, exist_ok=True)
    print("Export Gerbers + perçage…")
    export_gerbers()
    print("Export position (CPL)…")
    export_cpl()
    print("Export nomenclature (BOM)…")
    export_bom()
    print("Archive…")
    make_zip()
    print(f"\nFichiers de fabrication dans {OUT}")


if __name__ == "__main__":
    main()
