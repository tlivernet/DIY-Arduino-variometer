#!/usr/bin/env python3
"""Convertit les PBM produits par render_screens en PNG lisibles.

Utilise uniquement la bibliothèque standard (zlib est intégré à Python), donc
aucune dépendance à installer.

Le rendu est agrandi et les couleurs inversées pour ressembler à ce que donne un
Sharp Memory LCD : pixels sombres sur fond clair réfléchissant.
"""

import struct
import sys
import zlib
from pathlib import Path

SCALE = 2
# Fond gris très clair plutôt que blanc pur : un écran réfléchissant ne renvoie
# jamais 100 % de la lumière, et cela rend l'aperçu plus fidèle.
BACKGROUND = 0xEC
FOREGROUND = 0x11


def read_pbm(path: Path):
    """Lit un PBM binaire (P4) et renvoie (largeur, hauteur, octets)."""
    data = path.read_bytes()
    if not data.startswith(b"P4"):
        raise ValueError(f"{path} n'est pas un PBM binaire (P4)")

    # En-tête : jetons séparés par des espaces, commentaires '#' ignorés.
    pos = 2
    fields = []
    while len(fields) < 2:
        while pos < len(data) and data[pos : pos + 1].isspace():
            pos += 1
        if data[pos : pos + 1] == b"#":
            while pos < len(data) and data[pos] != 0x0A:
                pos += 1
            continue
        start = pos
        while pos < len(data) and not data[pos : pos + 1].isspace():
            pos += 1
        fields.append(int(data[start:pos]))
    pos += 1  # l'unique espace après l'en-tête

    width, height = fields
    return width, height, data[pos:]


def write_png(path: Path, width: int, height: int, rows: list[bytes]):
    """Écrit un PNG en niveaux de gris 8 bits."""

    def chunk(tag: bytes, payload: bytes) -> bytes:
        return (
            struct.pack(">I", len(payload))
            + tag
            + payload
            + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
        )

    # Bit depth 8, type 0 (niveaux de gris).
    header = struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)
    raw = b"".join(b"\x00" + row for row in rows)  # filtre 0 par ligne

    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", header)
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )
    path.write_bytes(png)


def convert(pbm: Path) -> Path:
    width, height, bits = read_pbm(pbm)
    stride = (width + 7) // 8

    rows = []
    for y in range(height):
        line = bytearray()
        base = y * stride
        for x in range(width):
            byte = bits[base + (x >> 3)]
            on = (byte >> (7 - (x & 7))) & 1
            line.extend([FOREGROUND if on else BACKGROUND] * SCALE)
        rows.extend([bytes(line)] * SCALE)

    png = pbm.with_suffix(".png")
    write_png(png, width * SCALE, height * SCALE, rows)
    return png


def main() -> int:
    target = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    files = sorted(target.glob("*.pbm")) if target.is_dir() else [target]

    if not files:
        print(f"aucun .pbm trouvé dans {target}", file=sys.stderr)
        return 1

    for pbm in files:
        png = convert(pbm)
        print(f"{pbm.name} -> {png.name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
