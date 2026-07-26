#!/usr/bin/env python3
"""Enregistre les trames brutes émises par le firmware sur l'USB, en CSV.

Le firmware, quand on lui envoie la commande « r » sur le port série, se met à
cracher une ligne CSV par trame de capteurs (~100 Hz, ~4 ko/s — rien pour de
l'USB natif). Ce script lit ce flux et l'enregistre tel quel, pour le rejouer
ensuite hors ligne avec tools/replay.

    tools/record.py --port /dev/ttyACM0 --seconds 30 -o traces/vol.csv

Le fichier obtenu se rejoue directement :

    make replay TRACE=traces/vol.csv

Dépend de pyserial (`pip install pyserial`). Volontairement la seule dépendance,
et seulement pour ce script d'acquisition — le rejoue et les tests, eux, ne
dépendent de rien.
"""

import argparse
import sys
import time
from pathlib import Path

# En-tête attendu, identique à core/raw_frame.cpp (kRawFrameCsvHeader).
EXPECTED_HEADER = "t_us,ax,ay,az,gx,gy,gz,baro,p_hpa,temp_c"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="port série (ex. /dev/ttyACM0)")
    parser.add_argument("--baud", type=int, default=115200, help="débit série")
    parser.add_argument(
        "--seconds",
        type=float,
        default=30.0,
        help="durée d'enregistrement en secondes (0 = jusqu'à Ctrl-C)",
    )
    parser.add_argument(
        "-o", "--output", required=True, type=Path, help="fichier CSV de sortie"
    )
    parser.add_argument(
        "--no-command",
        action="store_true",
        help="ne pas envoyer 'r' : le firmware émet déjà sans qu'on le demande",
    )
    args = parser.parse_args()

    try:
        import serial  # pyserial
    except ImportError:
        print(
            "pyserial est requis : pip install pyserial",
            file=sys.stderr,
        )
        return 1

    args.output.parent.mkdir(parents=True, exist_ok=True)

    try:
        link = serial.Serial(args.port, args.baud, timeout=1.0)
    except serial.SerialException as exc:
        print(f"Ouverture de {args.port} impossible : {exc}", file=sys.stderr)
        return 1

    # Laisse la carte redémarrer si l'ouverture du port l'a réinitialisée.
    time.sleep(0.3)
    link.reset_input_buffer()
    if not args.no_command:
        link.write(b"r\n")

    header_written = False
    lines = 0
    start = time.monotonic()

    with args.output.open("w", encoding="ascii", newline="\n") as out:
        try:
            while True:
                if args.seconds > 0 and time.monotonic() - start >= args.seconds:
                    break
                raw = link.readline()
                if not raw:
                    continue
                text = raw.decode("ascii", errors="ignore").strip()
                if not text:
                    continue
                # On ne garde que les lignes de données et l'en-tête ; les
                # messages de diagnostic du firmware sont écartés.
                if text == EXPECTED_HEADER:
                    if not header_written:
                        out.write(text + "\n")
                        header_written = True
                    continue
                if text[0].isdigit():
                    if not header_written:
                        out.write(EXPECTED_HEADER + "\n")
                        header_written = True
                    out.write(text + "\n")
                    lines += 1
        except KeyboardInterrupt:
            pass
        finally:
            link.close()

    print(f"{lines} trames écrites dans {args.output}", file=sys.stderr)
    return 0 if lines else 1


if __name__ == "__main__":
    raise SystemExit(main())
