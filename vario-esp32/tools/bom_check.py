#!/usr/bin/env python3
"""Valide les nomenclatures et produit les variantes à téléverser.

Une nomenclature qui part chez un distributeur avec une référence vide, une
quantité fausse ou un doublon de repère coûte une commande ratée. Ce script
attrape ces cas avant l'envoi.

Les fichiers dérivés sont ENGENDRÉS et non maintenus à la main : chaque
distributeur veut ses colonnes, et entretenir quatre copies d'une même
nomenclature garantit qu'elles divergent.

    python3 tools/bom_check.py            # valide seulement
    python3 tools/bom_check.py --emit     # + écrit tous les formats dérivés
"""

import argparse
import csv
import sys
from collections import Counter
from pathlib import Path

BOM_DIR = Path(__file__).resolve().parent.parent / "hardware" / "bom"

REQUIRED = ["Designator", "Quantity", "Manufacturer", "MPN", "Description"]

# Octopart, TrustedParts et FindChips se contentent de ces trois colonnes ;
# fournir le strict nécessaire évite qu'un champ mal interprété fasse échouer
# l'appariement.
UPLOAD_COLUMNS = ["MPN", "Manufacturer", "Quantity"]

# LCSC exige au minimum une colonne de quantité et une colonne d'identification
# (référence LCSC, référence fabricant ou description). Plus on en fournit,
# meilleur est l'appariement. Limites de leur outil : 4 Mo et 800 lignes.
LCSC_COLUMNS = ["Quantity", "Manufacture Part Number", "Manufacturer",
                "Description", "Designator", "LCSC Part Number"]

# JLCPCB, pour un assemblage, veut exactement ces quatre champs.
JLCPCB_COLUMNS = ["Comment", "Designator", "Footprint", "LCSC Part #"]

# Suffixes des fichiers ENGENDRÉS. Ils doivent être exclus de la découverte des
# sources, sans quoi un second passage les reprendrait comme nomenclatures et
# engendrerait des fichiers en cascade.
GENERATED_SUFFIXES = ("-upload", "-lcsc", "-jlcpcb")


def load(path: Path) -> list[dict]:
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def check(path: Path) -> list[str]:
    problems: list[str] = []
    rows = load(path)

    if not rows:
        return [f"{path.name}: fichier vide"]

    missing = [c for c in REQUIRED if c not in rows[0]]
    if missing:
        problems.append(f"{path.name}: colonnes manquantes {missing}")
        return problems

    seen_designators: Counter[str] = Counter()
    total_lines = 0

    for index, row in enumerate(rows, start=2):
        where = f"{path.name}:{index}"

        mpn = (row.get("MPN") or "").strip()
        if not mpn:
            problems.append(f"{where}: référence (MPN) vide")

        manufacturer = (row.get("Manufacturer") or "").strip()
        if not manufacturer:
            problems.append(f"{where}: fabricant vide")

        designator = (row.get("Designator") or "").strip()
        if not designator:
            problems.append(f"{where}: repère vide")
        seen_designators[designator] += 1

        raw_quantity = (row.get("Quantity") or "").strip()
        try:
            quantity = int(raw_quantity)
            if quantity < 1:
                problems.append(f"{where}: quantité {quantity} invalide")
            total_lines += quantity
        except ValueError:
            problems.append(f"{where}: quantité « {raw_quantity} » non entière")

        if not (row.get("Description") or "").strip():
            problems.append(f"{where}: description vide")

        # Les champs non ASCII passent mal dans les téléversements de certains
        # distributeurs : on le signale sans bloquer.
        for column in ("MPN", "Manufacturer"):
            value = row.get(column) or ""
            if any(ord(c) > 127 for c in value):
                problems.append(f"{where}: {column} contient des caractères non ASCII")

    for designator, count in seen_designators.items():
        if count > 1:
            problems.append(f"{path.name}: repère « {designator} » répété {count} fois")

    print(f"{path.name}: {len(rows)} lignes, {total_lines} composants au total")
    return problems


def write(out: Path, columns: list[str], records: list[dict], encoding="ascii") -> Path:
    with out.open("w", newline="", encoding=encoding) as handle:
        writer = csv.DictWriter(handle, fieldnames=columns)
        writer.writeheader()
        writer.writerows(records)
    print(f"  -> {out.name}")
    return out


def emit_upload(path: Path, rows: list[dict]) -> None:
    """Format générique : Octopart, TrustedParts, FindChips."""
    records = [{c: (r.get(c) or "").strip() for c in UPLOAD_COLUMNS} for r in rows]
    write(path.with_name(path.stem + "-upload.csv"), UPLOAD_COLUMNS, records)


def emit_lcsc(path: Path, rows: list[dict]) -> None:
    """Format de l'outil BOM de LCSC.

    La colonne « LCSC Part Number » est laissée VIDE à dessein : ces références
    en C… ne peuvent pas être devinées, et en inventer une ferait commander la
    mauvaise pièce sans que rien ne le signale. L'outil de LCSC la remplit lui-
    même depuis la référence fabricant.
    """
    records = []
    for r in rows:
        records.append({
            "Quantity": (r.get("Quantity") or "").strip(),
            "Manufacture Part Number": (r.get("MPN") or "").strip(),
            "Manufacturer": (r.get("Manufacturer") or "").strip(),
            "Description": (r.get("Description") or "").strip(),
            "Designator": (r.get("Designator") or "").strip(),
            "LCSC Part Number": "",
        })
    write(path.with_name(path.stem + "-lcsc.csv"), LCSC_COLUMNS, records)


def emit_jlcpcb(path: Path, rows: list[dict]) -> None:
    """Format d'assemblage JLCPCB.

    Incomplet par nature à ce stade : les références LCSC et les empreintes
    définitives ne seront connues qu'avec le schéma KiCad. Le fichier sert à
    figer le format, pas à passer commande.
    """
    records = []
    for r in rows:
        records.append({
            "Comment": (r.get("MPN") or "").strip(),
            "Designator": (r.get("Designator") or "").strip(),
            "Footprint": (r.get("Package") or "").strip(),
            "LCSC Part #": "",
        })
    write(path.with_name(path.stem + "-jlcpcb.csv"), JLCPCB_COLUMNS, records)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--emit", "--emit-upload", action="store_true", dest="emit",
                        help="écrit tous les formats dérivés (générique, LCSC, JLCPCB)")
    args = parser.parse_args()

    files = sorted(p for p in BOM_DIR.glob("*.csv")
                   if not p.stem.endswith(GENERATED_SUFFIXES))
    if not files:
        print(f"aucune nomenclature dans {BOM_DIR}", file=sys.stderr)
        return 1

    problems: list[str] = []
    for path in files:
        problems.extend(check(path))
        if not args.emit:
            continue

        rows = load(path)
        emit_upload(path, rows)
        emit_lcsc(path, rows)
        # L'assemblage ne concerne que la nomenclature au composant : les
        # modules de la variante A se soudent à la main.
        if "composants" in path.stem:
            emit_jlcpcb(path, rows)

    if problems:
        print("\nProblèmes :", file=sys.stderr)
        for problem in problems:
            print(f"  - {problem}", file=sys.stderr)
        return 1

    print("\nNomenclatures valides.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
