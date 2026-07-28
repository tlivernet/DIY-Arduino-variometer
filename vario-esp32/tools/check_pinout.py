#!/usr/bin/env python3
"""Vérifie que le tableau de câblage et le firmware disent la même chose.

Un plan de câblage qui diverge de `src/app/pins.h` est pire que pas de plan du
tout : on soude d'après le document, le firmware pilote d'autres broches, et le
symptôme ressemble à un composant défectueux. Ce contrôle rend la divergence
impossible.

Il vérifie aussi que les broches choisies sont réellement utilisables sur la
carte visée, ce qui ne se voit pas en lisant le code.

    python3 tools/check_pinout.py
"""

import csv
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PINS_H = ROOT / "src" / "app" / "pins.h"
WIRING = ROOT / "hardware" / "carte-porteuse" / "brochage.csv"

# Correspondance entre les constantes du firmware et les signaux du tableau.
SIGNAL_OF_CONSTANT = {
    "kI2cSda": "I2C_SDA",
    "kI2cScl": "I2C_SCL",
    "kDisplaySclk": "DISP_SCLK",
    "kDisplayMosi": "DISP_MOSI",
    "kDisplayCs": "DISP_CS",
    "kDisplayDisp": "DISP_ON",
    "kDisplayExtcomin": "DISP_VCOM",
    "kBuzzerA": "BUZZ_A",
    "kBuzzerB": "BUZZ_B",
    "kButtonUp": "BTN_UP",
    "kButtonOk": "BTN_OK",
    "kButtonDown": "BTN_DOWN",
    "kBatterySense": "BAT_SENSE",
    "kImuInterrupt": "IMU_INT",
}

# Broches à ne pas utiliser sur une Waveshare ESP32-S3-Zero, avec la raison.
#
# La carte expose 24 GPIO. Les GPIO 33 à 37 ne sont PAS sortis : ils sont
# réservés à la PSRAM octale. Les autres restrictions viennent de l'ESP32-S3
# lui-même.
FORBIDDEN = {
    0: "broche d'amorçage",
    19: "USB natif D-",
    20: "USB natif D+",
    21: "LED RGB WS2812 de la carte",
    26: "flash SPI interne",
    27: "flash SPI interne",
    28: "flash SPI interne",
    29: "flash SPI interne",
    30: "flash SPI interne",
    31: "flash SPI interne",
    32: "flash SPI interne",
    33: "non sortie sur la carte (PSRAM octale)",
    34: "non sortie sur la carte (PSRAM octale)",
    35: "non sortie sur la carte (PSRAM octale)",
    36: "non sortie sur la carte (PSRAM octale)",
    37: "non sortie sur la carte (PSRAM octale)",
    43: "UART0 TX, utilisé par la console série",
    44: "UART0 RX, utilisé par la console série",
    45: "broche de configuration au démarrage",
    46: "broche de configuration au démarrage",
}

# Seul l'ADC1 est utilisable quand le Wi-Fi est actif.
ADC1_PINS = set(range(1, 11))


def read_firmware_pins() -> dict[str, int]:
    text = PINS_H.read_text(encoding="utf-8")
    found = {}
    for name, value in re.findall(r"constexpr int (k\w+)\s*=\s*(\d+)", text):
        found[name] = int(value)
    return found


def read_wiring() -> list[dict]:
    with WIRING.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def main() -> int:
    problems: list[str] = []

    firmware = read_firmware_pins()
    wiring = read_wiring()

    # Signal -> GPIO, tel que décrit par le tableau de câblage.
    wiring_pins: dict[str, int] = {}
    signal_rows: dict[str, list[str]] = defaultdict(list)

    for row in wiring:
        raw = (row.get("GPIO") or "").strip()
        signal = (row.get("Signal") or "").strip()
        if not raw:
            continue  # ligne d'alimentation
        try:
            gpio = int(raw)
        except ValueError:
            problems.append(f"câblage : GPIO « {raw} » non numérique pour {signal}")
            continue

        signal_rows[signal].append(row.get("Bloc", "?"))

        if signal in wiring_pins and wiring_pins[signal] != gpio:
            problems.append(
                f"câblage : le signal {signal} apparaît sur GPIO "
                f"{wiring_pins[signal]} et {gpio}"
            )
        wiring_pins[signal] = gpio

    # 1. Chaque constante du firmware doit exister dans le câblage, au même GPIO.
    for constant, signal in SIGNAL_OF_CONSTANT.items():
        if constant not in firmware:
            problems.append(f"pins.h : constante {constant} absente")
            continue
        if signal not in wiring_pins:
            problems.append(f"câblage : signal {signal} absent (attendu pour {constant})")
            continue
        if firmware[constant] != wiring_pins[signal]:
            problems.append(
                f"DIVERGENCE : {constant} = GPIO {firmware[constant]} dans pins.h, "
                f"mais {signal} = GPIO {wiring_pins[signal]} dans le câblage"
            )

    # 2. Aucun GPIO ne doit servir à deux signaux différents.
    by_gpio: dict[int, list[str]] = defaultdict(list)
    for signal, gpio in wiring_pins.items():
        by_gpio[gpio].append(signal)
    for gpio, signals in sorted(by_gpio.items()):
        if len(signals) > 1:
            problems.append(f"conflit : GPIO {gpio} partagé par {', '.join(sorted(signals))}")

    # 3. Les broches interdites sur la carte visée.
    for signal, gpio in sorted(wiring_pins.items()):
        if gpio in FORBIDDEN:
            problems.append(f"GPIO {gpio} ({signal}) inutilisable : {FORBIDDEN[gpio]}")

    # 4. La mesure de batterie doit être sur ADC1.
    battery = wiring_pins.get("BAT_SENSE")
    if battery is not None and battery not in ADC1_PINS:
        problems.append(
            f"GPIO {battery} (BAT_SENSE) n'est pas sur ADC1 : l'ADC2 est "
            "inutilisable quand le Wi-Fi est actif"
        )

    # Bilan.
    used = sorted(wiring_pins.values())
    print(f"pins.h : {len(firmware)} constantes")
    print(f"câblage : {len(wiring)} lignes, {len(wiring_pins)} signaux, "
          f"{len(used)} GPIO utilisés")
    print(f"GPIO : {', '.join(str(g) for g in used)}")

    if problems:
        print("\nProblèmes :", file=sys.stderr)
        for problem in problems:
            print(f"  - {problem}", file=sys.stderr)
        return 1

    print("\nLe câblage et le firmware concordent.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
