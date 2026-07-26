#!/usr/bin/env python3
"""Valide les nomenclatures et produit les variantes à téléverser.

Une nomenclature qui part chez un distributeur avec une référence vide, une
quantité fausse ou un doublon de repère coûte une commande ratée. Ce script
attrape ces cas avant l'envoi.

    python3 tools/bom_check.py                       # valide
    python3 tools/bom_check.py --emit-upload         # écrit les CSV minimaux
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


def emit_upload(path: Path) -> Path:
    rows = load(path)
    out = path.with_name(path.stem + "-upload.csv")

    with out.open("w", newline="", encoding="ascii") as handle:
        writer = csv.DictWriter(handle, fieldnames=UPLOAD_COLUMNS)
        writer.writeheader()
        for row in rows:
            writer.writerow({c: (row.get(c) or "").strip() for c in UPLOAD_COLUMNS})

    print(f"  -> {out.name}")
    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--emit-upload", action="store_true",
                        help="écrit les CSV réduits aux colonnes de téléversement")
    args = parser.parse_args()

    files = sorted(p for p in BOM_DIR.glob("*.csv") if not p.stem.endswith("-upload"))
    if not files:
        print(f"aucune nomenclature dans {BOM_DIR}", file=sys.stderr)
        return 1

    problems: list[str] = []
    for path in files:
        problems.extend(check(path))
        if args.emit_upload:
            emit_upload(path)

    if problems:
        print("\nProblèmes :", file=sys.stderr)
        for problem in problems:
            print(f"  - {problem}", file=sys.stderr)
        return 1

    print("\nNomenclatures valides.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
