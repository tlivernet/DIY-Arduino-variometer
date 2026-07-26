#!/usr/bin/env python3
"""Engendre le schéma KiCad du variomètre (variante B, PCB intégré) — étape P7.

Le schéma est ENGENDRÉ, pas dessiné à la main, dans le même esprit que le reste
du dépôt : la nomenclature est engendrée, les écrans sont rendus. La source de
vérité est la table de connexions CONNECTIONS/TWO_PIN/BUTTONS ci-dessous — un
mapping composant → (broche → net). La connectivité passe par des LABELS GLOBAUX
homonymes posés à l'extrémité connectable de chaque broche : deux broches
portant le même nom de net sont reliées, sans fil à tracer. Cela rend le
brochage auditable ligne par ligne et le fichier régénérable.

    python3 generate_schematic.py         # écrit vario.kicad_sch
    kicad-cli sch erc vario.kicad_sch     # doit être propre

Le placement est volontairement sur une grille large : le schéma est correct
électriquement (l'ERC le valide) mais pas mis en page pour la lecture humaine.
Le peaufinage graphique se fera dans l'éditeur avant le routage du PCB.

Brochages vérifiés le 2026-07-26 contre les bibliothèques KiCad 9 système et,
pour le TPS61023, contre le datasheet TI SLVSF14B (page 3, package DRL/SOT563).
Affectation des GPIO reprise de src/app/pins.h, les numéros de GPIO logiques
étant identiques sur le module WROOM-1.
"""

import hashlib
from pathlib import Path

import sexp as sp

SYM_DIR = Path("/usr/share/kicad/symbols")
OUT = Path(__file__).resolve().parent / "vario.kicad_sch"
PROJECT = "vario"
SHEET_UUID = "a1b2c3d4-0000-4000-8000-000000000001"

# ---------------------------------------------------------------------------
# Table de connexions. lib = "Fichier:Symbole" (fichier système) ou
# "vario-symbols:Symbole" (bibliothèque projet). nets : {num_broche: net}.
# Un net "" laisse la broche non connectée (un no_connect est posé).
# ---------------------------------------------------------------------------

CONNECTIONS = {
    "U1": {
        "lib": "RF_Module:ESP32-S3-WROOM-1",
        "value": "ESP32-S3-WROOM-1-N16R8",
        "footprint": "RF_Module:ESP32-S3-WROOM-1",
        "nets": {
            "1": "GND", "40": "GND", "41": "GND",
            "2": "+3V3",
            "3": "EN",
            "12": "SDA",           # IO8
            "17": "SCL",           # IO16 (SCL logique)
            "19": "DISP_MOSI",     # IO11
            "20": "DISP_SCLK",     # IO12
            "18": "DISP_CS",       # IO10
            "21": "DISP_DISP",     # IO13
            "22": "DISP_EXTCOMIN", # IO14
            "39": "BUZZER_A",      # IO1
            "38": "BUZZER_B",      # IO2
            "4": "BTN_UP_IO",      # IO4
            "5": "BTN_OK",         # IO5
            "6": "BTN_DOWN_IO",    # IO6
            "15": "VBAT_SENSE",    # IO3 (ADC1)
            "7": "IMU_INT1",       # IO7
            "13": "USB_D-", "14": "USB_D+",
        },
    },
    "U2": {
        "lib": "Sensor_Pressure:MS5611-01BA",
        "value": "MS5611-01BA03",
        "footprint": "Package_LGA:LGA-8_3x5mm_P1.25mm",
        "nets": {
            "1": "+3V3", "3": "GND",
            "2": "+3V3",           # PS = 1 : I2C
            "4": "GND", "5": "GND",  # CSB = 0
            "6": "MS_SDO",         # SDO : sélection d'adresse via R15 (0 Ω) vers GND
            "7": "SDA", "8": "SCL",
        },
    },
    "U3": {
        "lib": "Sensor_Motion:LSM6DS3",
        "value": "LSM6DS3TR-C",
        "footprint": "Package_LGA:LGA-14_3x2.5mm_P0.5mm_LayoutBorder3x4y",
        "nets": {
            "8": "+3V3", "5": "+3V3",
            "6": "GND", "7": "GND",
            "12": "+3V3",          # CS = 1 : I2C
            "1": "IMU_SA0",        # SDO/SA0 : sélection d'adresse via R16 (0 Ω) vers GND
            "13": "SCL", "14": "SDA",
            "4": "IMU_INT1",
            "9": "", "2": "", "3": "", "10": "", "11": "",
        },
    },
    "U4": {
        "lib": "Timer_RTC:DS3231M",
        "value": "DS3231SN",
        "footprint": "Package_SO:SOIC-16W_7.5x10.3mm_P1.27mm",
        "nets": {
            "2": "+3V3", "13": "GND",
            "5": "GND", "6": "GND", "7": "GND", "8": "GND", "9": "GND",
            "10": "GND", "11": "GND", "12": "GND",
            "14": "VBACKUP",
            "15": "SDA", "16": "SCL",
            "3": "RTC_INT",
            "1": "", "4": "",
        },
    },
    "U5": {
        "lib": "Battery_Management:MCP73831-2-OT",
        "value": "MCP73831T-2ACI/OT",
        "footprint": "Package_TO_SOT_SMD:SOT-23-5",
        "nets": {
            "4": "VBUS", "2": "GND", "3": "+VBATT",
            "1": "CHG_STAT", "5": "CHG_PROG",
        },
    },
    "U6": {
        "lib": "vario-symbols:TPS61023DRL",
        "value": "TPS61023DRLR",
        "footprint": "Package_TO_SOT_SMD:SOT-563",
        "nets": {
            "3": "+VBATT", "4": "GND", "2": "+VBATT",
            "5": "SW", "6": "+5V", "1": "BOOST_FB",
        },
    },
    "U7": {
        "lib": "Regulator_Linear:TLV75733PDBV",
        "value": "TLV75733PDBVR",
        "footprint": "Package_TO_SOT_SMD:SOT-23-5",
        "nets": {
            "1": "+5V", "2": "GND", "5": "+3V3", "3": "+5V", "4": "",
        },
    },
    # Tampon audio : DEUX buffers single-gate 74HCT1G125 (au lieu d'un dual
    # 74HCT2G34, introuvable chez LCSC). Contrainte du projet : famille HCT
    # (seuils TTL, seuil haut ~2 V) pour qu'une sortie 3,3 V de l'ESP32 commande
    # de façon fiable un buffer alimenté en 5 V. Le 1G125 est un buffer à sortie
    # trois-états : sa broche de validation OE̅ (active à l'état bas) est câblée
    # à la masse pour le rendre toujours passant. Deux mono-buffers valent le
    # double pour le montage push-pull ; le déphasage vient des deux signaux
    # LEDC BUZZER_A / BUZZER_B côté firmware. LCSC C547448 (74HCT1G125GW).
    "U8": {
        "lib": "74xGxx:74AHCT1G125",
        "value": "74HCT1G125",
        "footprint": "Package_TO_SOT_SMD:SOT-353_SC-70-5",
        "nets": {
            "5": "+5V", "3": "GND",
            "1": "GND",           # OE̅ à la masse : buffer toujours actif
            "2": "BUZZER_A", "4": "PIEZO_A",
        },
    },
    "U9": {
        "lib": "74xGxx:74AHCT1G125",
        "value": "74HCT1G125",
        "footprint": "Package_TO_SOT_SMD:SOT-353_SC-70-5",
        "nets": {
            "5": "+5V", "3": "GND",
            "1": "GND",
            "2": "BUZZER_B", "4": "PIEZO_B",
        },
    },
    "J1": {
        "lib": "Connector:USB_C_Receptacle_USB2.0_16P",
        "value": "USB4105-GF-A",
        "footprint": "Connector_USB:USB_C_Receptacle_GCT_USB4105-xx-A_16P_TopMnt_Horizontal",
        "nets": {
            "A1": "GND", "B1": "GND", "A12": "GND", "B12": "GND",
            "A4": "VBUS", "B4": "VBUS", "A9": "VBUS", "B9": "VBUS",
            "A5": "USB_CC1", "B5": "USB_CC2",
            "A6": "USB_D+", "B6": "USB_D+",
            "A7": "USB_D-", "B7": "USB_D-",
            "A8": "", "B8": "",       # SBU1/SBU2 non utilisés
            "S1": "GND",              # blindage à la masse
        },
    },
    "J3": {
        "lib": "Connector_Generic:Conn_01x10",
        "value": "FPC-10 LS027B7DH01",
        "footprint": "Connector_FFC-FPC:Hirose_FH12-10S-0.5SH_1x10-1MP_P0.50mm_Horizontal",
        "nets": {
            "1": "GND",
            "2": "DISP_CS",
            "3": "DISP_MOSI",
            "4": "DISP_SCLK",
            "5": "DISP_EXTCOMIN",
            "6": "DISP_DISP",
            "7": "+5V", "8": "+5V",
            "9": "GND", "10": "GND",
        },
    },
}

# (repère, valeur, net_pin1, net_pin2). Empreinte 0603 par défaut sauf indiqué.
TWO_PIN = [
    ("C1", "100n", "Device:C", "0603", "+3V3", "GND"),
    ("C2", "100n", "Device:C", "0603", "+3V3", "GND"),
    ("C3", "100n", "Device:C", "0603", "+3V3", "GND"),
    ("C4", "100n", "Device:C", "0603", "+3V3", "GND"),
    ("C5", "100n", "Device:C", "0603", "+5V", "GND"),
    ("C6", "100n", "Device:C", "0603", "+VBATT", "GND"),
    ("C7", "10u", "Device:C", "0603", "+3V3", "GND"),
    ("C8", "10u", "Device:C", "0603", "+5V", "GND"),
    ("C9", "10u", "Device:C", "0603", "+VBATT", "GND"),
    ("C10", "10u", "Device:C", "0603", "VBUS", "GND"),
    ("C11", "22u", "Device:C", "0603", "+VBATT", "GND"),
    ("C12", "22u", "Device:C", "0603", "+5V", "GND"),
    ("C13", "1u", "Device:C", "0603", "+5V", "GND"),
    ("C14", "1u", "Device:C", "0603", "+3V3", "GND"),
    ("C15", "1u", "Device:C", "0603", "EN", "GND"),
    ("L1", "2.2u", "Device:L", "L_0603", "+VBATT", "SW"),
    ("R1", "10k", "Device:R", "0603", "+3V3", "SDA"),
    ("R2", "10k", "Device:R", "0603", "+3V3", "SCL"),
    ("R3", "100k", "Device:R", "0603", "+VBATT", "VBAT_SENSE"),
    ("R4", "100k", "Device:R", "0603", "VBAT_SENSE", "GND"),
    ("R5", "2k", "Device:R", "0603", "CHG_PROG", "GND"),
    ("R6", "5.1k", "Device:R", "0603", "USB_CC1", "GND"),
    ("R7", "5.1k", "Device:R", "0603", "USB_CC2", "GND"),
    ("R8", "1k", "Device:R", "0603", "BTN_UP", "BTN_UP_IO"),
    ("R9", "1k", "Device:R", "0603", "BTN_DOWN", "BTN_DOWN_IO"),
    ("R10", "910k", "Device:R", "0603", "+5V", "BOOST_FB"),
    ("R11", "100k", "Device:R", "0603", "BOOST_FB", "GND"),
    ("R12", "10k", "Device:R", "0603", "+3V3", "EN"),
    ("R13", "10k", "Device:R", "0603", "+3V3", "CHG_STAT"),   # tirage témoin charge
    ("R14", "10k", "Device:R", "0603", "+3V3", "RTC_INT"),    # tirage ~INT/SQW
    # Sélection d'adresse des capteurs : SDO/SA0 tirés à GND par 0 Ω. Un pont
    # nul plutôt qu'un fil direct — l'adresse reste modifiable au montage, et
    # cela évite qu'une sortie du capteur attaque directement le rail GND.
    ("R15", "0", "Device:R", "0603", "MS_SDO", "GND"),
    ("R16", "0", "Device:R", "0603", "IMU_SA0", "GND"),
    # Protection ESD sur l'entrée USB : TVS bidirectionnelle VBUS ↔ GND.
    ("D1", "SMF5.0A", "Device:D_TVS", "Diode_SMD:D_SOD-323", "VBUS", "GND"),
    ("LS1", "PIEZO 20mm", "Device:Buzzer", "Buzzer", "PIEZO_A", "PIEZO_B"),
    ("J2", "JST-PH-2", "Connector:Conn_01x02_Pin", "JST", "+VBATT", "GND"),
    ("BAT2", "CR1220", "Device:Battery_Cell", "BAT", "VBACKUP", "GND"),
]

# Boutons : SPST tactile, un côté au net, l'autre à GND.
BUTTONS = [
    ("SW1", "SW_Push", "BTN_UP", "GND"),
    ("SW2", "SW_Push", "BTN_OK", "GND"),
    ("SW3", "SW_Push", "BTN_DOWN", "GND"),
]

# Interrupteur marche/arrêt : coupe l'accu. Modélisé en série VBATT_RAW->VBATT.
SLIDE = [("SW4", "SW_SPDT", "+VBATT", "+VBATT", "GND")]

# PWR_FLAG uniquement sur les rails SANS broche « power_out » pilotante,
# sinon l'ERC signale deux sources sur le net. GND n'a que des power_in,
# VBACKUP et VBUS viennent d'un connecteur (pins passives). Les rails +5V,
# +3V3, +VBATT sont déjà pilotés respectivement par VOUT du boost, OUT du LDO
# et V_BAT du chargeur : ils ne portent pas de flag.
POWER_FLAGS = ["GND", "VBUS", "VBACKUP"]

# Référence fabricant par repère, reportée en propriété de symbole pour que
# l'analyseur et l'export BOM voient une couverture MPN. Source : la
# nomenclature hardware/bom/bom-composants.csv (source de vérité). Les passifs
# groupés y sont référencés par valeur ; on reporte ici leur MPN représentatif.
MPNS = {
    "U1": ("Espressif Systems", "ESP32-S3-WROOM-1-N16R8"),
    "U2": ("TE Connectivity", "MS5611-01BA03"),
    "U3": ("STMicroelectronics", "LSM6DS3TR-C"),
    "U4": ("Analog Devices", "DS3231SN#"),
    "U5": ("Microchip Technology", "MCP73831T-2ACI/OT"),
    "U6": ("Texas Instruments", "TPS61023DRLR"),
    "U7": ("Texas Instruments", "TLV75733PDBVR"),
    "U8": ("Nexperia", "74HCT1G125GW"),
    "U9": ("Nexperia", "74HCT1G125GW"),
    "J1": ("GCT", "USB4105-GF-A"),
    "J2": ("JST", "S2B-PH-K-S"),
    "J3": ("Hirose", "FH34SRJ-10S-0.5SH(50)"),
    "L1": ("Murata", "DFE252012F-2R2M=P2"),
    "LS1": ("Murata", "7BB-20-6L0"),
    "BAT2": ("Q&J", "CR1220-2"),   # support de pile CR1220 (pile non fournie)
    "SW1": ("Kaihong", "KH-6X6X4.3H-STM"),
    "SW2": ("Kaihong", "KH-6X6X4.3H-STM"),
    "SW3": ("Kaihong", "KH-6X6X4.3H-STM"),
    # Passifs : MPN représentatif par valeur (voir BOM pour le détail groupé).
    "C1": ("Samsung", "CL10B104KB8NNNC"), "C2": ("Samsung", "CL10B104KB8NNNC"),
    "C3": ("Samsung", "CL10B104KB8NNNC"), "C4": ("Samsung", "CL10B104KB8NNNC"),
    "C5": ("Samsung", "CL10B104KB8NNNC"), "C6": ("Samsung", "CL10B104KB8NNNC"),
    "C7": ("Samsung", "CL10A106MQ8NNNC"), "C8": ("Samsung", "CL10A106MQ8NNNC"),
    "C9": ("Samsung", "CL10A106MQ8NNNC"), "C10": ("Samsung", "CL10A106MQ8NNNC"),
    "C11": ("Murata", "GRM188R60J226MEA0D"), "C12": ("Murata", "GRM188R60J226MEA0D"),
    "C13": ("Samsung", "CL10A105KB8NNNC"), "C14": ("Samsung", "CL10A105KB8NNNC"),
    "C15": ("Samsung", "CL10A105KB8NNNC"),
    "R1": ("Yageo", "RC0603FR-0710KL"), "R2": ("Yageo", "RC0603FR-0710KL"),
    "R3": ("Yageo", "RC0603FR-07100KL"), "R4": ("Yageo", "RC0603FR-07100KL"),
    "R5": ("Yageo", "RC0603FR-072KL"),
    "R6": ("Yageo", "RC0603FR-075K1L"), "R7": ("Yageo", "RC0603FR-075K1L"),
    "R8": ("Yageo", "RC0603FR-071KL"), "R9": ("Yageo", "RC0603FR-071KL"),
    "R10": ("Yageo", "RC0603FR-07909KL"), "R11": ("Yageo", "RC0603FR-07100KL"),
    "R12": ("Yageo", "RC0603FR-0710KL"), "R13": ("Yageo", "RC0603FR-0710KL"),
    "R14": ("Yageo", "RC0603FR-0710KL"),
    "R15": ("Yageo", "RC0603FR-070RL"), "R16": ("Yageo", "RC0603FR-070RL"),
}

# Référence LCSC (Cxxxxx) par repère, requise pour l'assemblage JLCPCB —
# vérifiées en stock le 2026-07-26 via l'API jlcsearch. Les pièces actives sont
# « extended » (frais de pose ~3 $/référence unique) ; les passifs 0603 de la
# série Uni-Royal 0603WAF sont « basic » (sans frais). Une pièce sans réf ici
# devra être posée à la main (connecteurs, écran, piézo, accu).
LCSC = {
    "U1": "C2913202",   # ESP32-S3-WROOM-1-N16R8
    "U2": "C15639",     # MS5611-01BA03
    "U3": "C967633",    # LSM6DS3TR-C
    "U4": "C9866",      # DS3231SN
    "U5": "C424093",    # MCP73831T-2ACI/OT
    "U6": "C919459",    # TPS61023DRLR
    "U7": "C485517",    # TLV75733PDBVR
    "U8": "C547448",    # 74HCT1G125GW
    "U9": "C547448",    # 74HCT1G125GW
    "D1": "C19077497",  # SMF5.0A (TVS)
    "L1": "C576403",    # DFE252012F-2R2M (2.2 µH)
    # Connecteurs assemblables par JLCPCB (le reste — accu, piézo, écran — se
    # pose à la main, voir docs/pcb.md).
    "J1": "C3020560",   # USB4105-GF-A (USB-C)
    "J3": "C324723",    # FH34SRJ-10S-0.5SH — réf exacte (empreinte FH12 approchante)
    "BAT2": "C70381",   # support de pile CR1220 (la pile CR1220 se glisse à la main)
    "J2": "C173752",    # S2B-PH-K-S — embase accu JST-PH 2 voies (réf exacte)
    "SW1": "C2837530", "SW2": "C2837530", "SW3": "C2837530",  # bouton 6×6×4.3 mm
    # Condensateurs
    "C1": "C14663", "C2": "C14663", "C3": "C14663", "C4": "C14663",
    "C5": "C14663", "C6": "C14663",                      # 100 nF (basic)
    "C7": "C19702", "C8": "C19702", "C9": "C19702", "C10": "C19702",  # 10 µF
    "C11": "C59461", "C12": "C59461",                    # 22 µF (basic)
    "C13": "C15849", "C14": "C15849", "C15": "C15849",   # 1 µF
    # Résistances (Uni-Royal 0603WAF, basic)
    "R1": "C25804", "R2": "C25804",                      # 10 k
    "R3": "C25803", "R4": "C25803",                      # 100 k
    "R5": "C22975",                                       # 2 k
    "R6": "C23186", "R7": "C23186",                      # 5,1 k
    "R8": "C21190", "R9": "C21190",                      # 1 k
    "R10": "C23263",                                      # 910 k (ext)
    "R11": "C25803",                                      # 100 k
    "R12": "C25804", "R13": "C25804", "R14": "C25804",   # 10 k
    "R15": "C21189", "R16": "C21189",                    # 0 Ω
}


# ---------------------------------------------------------------------------
# Extraction des symboles
# ---------------------------------------------------------------------------

def _lib_path(libfile):
    if libfile == "vario-symbols":
        return Path(__file__).resolve().parent / "vario-symbols.kicad_sym"
    return SYM_DIR / f"{libfile}.kicad_sym"


def sym_block_text(libfile, name):
    """Texte S-expr équilibré du seul (symbol "name" ...)."""
    path = _lib_path(libfile)
    txt = path.read_text()
    key = f'(symbol "{name}"'
    i = txt.find(key)
    if i < 0:
        raise SystemExit(f"symbole introuvable : {name} dans {path}")
    depth = 0
    for k in range(i, len(txt)):
        if txt[k] == "(":
            depth += 1
        elif txt[k] == ")":
            depth -= 1
            if depth == 0:
                return txt[i : k + 1]
    raise SystemExit(f"bloc non équilibré : {name}")


def _symbols_dict(libfile):
    doc = sp.parse_file(str(_lib_path(libfile)))
    d = {}
    for node in doc:
        if (
            isinstance(node, list)
            and node
            and node[0] == "symbol"
            and len(node) > 1
            and isinstance(node[1], str)
        ):
            d[node[1]] = node
    return d


def _extends_of(libfile, name):
    d = _symbols_dict(libfile)
    for sub in d[name]:
        if isinstance(sub, list) and sub and sub[0] == "extends":
            return sub[1]
    return None


def pin_positions(libfile, name):
    """[(num, x, y, angle, length), ...] en suivant extends."""
    d = _symbols_dict(libfile)

    def rec(nm):
        out = []
        sym = d[nm]
        for sub in sym:
            if isinstance(sub, list) and sub and sub[0] == "extends":
                out += rec(sub[1])
            if isinstance(sub, list) and sub and sub[0] == "symbol":
                for pin in sub:
                    if isinstance(pin, list) and pin and pin[0] == "pin":
                        at = sp.find_first(pin, "at")
                        ln = sp.find_first(pin, "length")
                        num = None
                        for x2 in pin:
                            if isinstance(x2, list) and x2[0] == "number":
                                num = x2[1]
                        out.append(
                            (
                                num,
                                float(at[1]),
                                float(at[2]),
                                float(at[3]) if len(at) > 3 else 0.0,
                                float(ln[1]) if ln else 0.0,
                            )
                        )
        return out

    return rec(name)


# ---------------------------------------------------------------------------
# Émission S-expression
# ---------------------------------------------------------------------------

_uid_counter = [0]


def uid(seed=""):
    """UUID déterministe : régénérer le schéma ne bouscule pas les UUID."""
    _uid_counter[0] += 1
    h = hashlib.md5(f"{seed}:{_uid_counter[0]}".encode()).hexdigest()
    return f"{h[0:8]}-{h[8:12]}-4{h[13:16]}-8{h[17:20]}-{h[20:32]}"


def endpoint(inst_x, inst_y, px, py):
    """Extrémité connectable absolue d'une broche (instance à rotation 0).

    Règle vérifiée à l'ERC : ext = (inst_x + px, inst_y - py). L'axe Y du
    schéma est inversé par rapport aux coordonnées du symbole. (px, py) est
    déjà l'extrémité connectable dans le repère du symbole — l'angle et la
    longueur de la broche n'entrent pas dans le calcul.
    """
    return (round(inst_x + px, 4), round(inst_y - py, 4))


def emit_global_label(net, x, y):
    return (
        f'\t(global_label "{net}" (shape input) (at {x} {y} 0)\n'
        f"\t\t(effects (font (size 1.27 1.27)) (justify left))\n"
        f'\t\t(uuid "{uid(net)}"))\n'
    )


def emit_no_connect(x, y):
    return f'\t(no_connect (at {x} {y}) (uuid "{uid("nc")}"))\n'


# Empreinte réelle pour les composants à deux broches, depuis un code court.
# Les passifs 0603 se distinguent par le préfixe de repère (R = résistance,
# C = condensateur : empreintes différentes malgré le même boîtier).
def resolve_footprint(ref, short):
    if short == "0603":
        kind = "Resistor_SMD:R" if ref[0] == "R" else "Capacitor_SMD:C"
        return f"{kind}_0603_1608Metric"
    return {
        # DFE252012 (2,5 × 2,0 mm) : empreinte 2520 approchante.
        "L_0603": "Inductor_SMD:L_Cenker_CKCS252012",
        # S2B-PH-K : embase JST-PH 2 voies coudée.
        "JST": "Connector_JST:JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal",
        # Buzzer : disque piézo 20 mm nu — pose à la main, empreinte à créer.
        "Buzzer": "Buzzer_Beeper:Buzzer_Murata_PKLCS1212E",
        # Support de pile CR1220 (C70381, assemblable JLCPCB). Empreinte CR1225
        # approchante (même Ø 12 mm) : à confirmer contre le support réel.
        "BAT": "Battery:Battery_CR1225",
    }.get(short, short)


def emit_instance(ref, lib_id, value, footprint, x, y, extra_props=""):
    mpn_props = ""
    if ref in MPNS:
        mfr, mpn = MPNS[ref]
        mpn_props = (
            f'\t\t(property "MPN" "{mpn}" (at {x} {y} 0)\n'
            f"\t\t\t(effects (font (size 1.27 1.27)) (hide yes)))\n"
            f'\t\t(property "Manufacturer" "{mfr}" (at {x} {y} 0)\n'
            f"\t\t\t(effects (font (size 1.27 1.27)) (hide yes)))\n"
        )
    if ref in LCSC:
        mpn_props += (
            f'\t\t(property "LCSC" "{LCSC[ref]}" (at {x} {y} 0)\n'
            f"\t\t\t(effects (font (size 1.27 1.27)) (hide yes)))\n"
        )
    return (
        f'\t(symbol (lib_id "{lib_id}") (at {x} {y} 0) (unit 1)\n'
        f"\t\t(exclude_from_sim no) (in_bom yes) (on_board yes) (dnp no)\n"
        f'\t\t(uuid "{uid(ref)}")\n'
        f'\t\t(property "Reference" "{ref}" (at {x+2} {y-5} 0)\n'
        f"\t\t\t(effects (font (size 1.27 1.27)) (justify left)))\n"
        f'\t\t(property "Value" "{value}" (at {x+2} {y+5} 0)\n'
        f"\t\t\t(effects (font (size 1.27 1.27)) (justify left)))\n"
        f"{mpn_props}"
        f'\t\t(property "Footprint" "{footprint}" (at {x} {y} 0)\n'
        f"\t\t\t(effects (font (size 1.27 1.27)) (hide yes)))\n"
        f'\t\t(property "Datasheet" "~" (at {x} {y} 0)\n'
        f"\t\t\t(effects (font (size 1.27 1.27)) (hide yes)))\n"
        f"{extra_props}"
        f'\t\t(instances (project "{PROJECT}"\n'
        f'\t\t\t(path "/{SHEET_UUID}" (reference "{ref}") (unit 1)))))\n'
    )


def main():
    body = []

    # Grille de placement : colonnes espacées, une entrée par composant.
    GRID_X0, GRID_Y0 = 25.4, 25.4
    COL_STEP, ROW_STEP = 50.8, 38.1
    COLS = 6
    slot = [0]

    def place():
        i = slot[0]
        slot[0] += 1
        col = i % COLS
        row = i // COLS
        return GRID_X0 + col * COL_STEP, GRID_Y0 + row * ROW_STEP

    lib_defs = {}   # lib_id -> (block_text, nom_source, nom_cible)

    def ensure_lib(libfile, name):
        lib_id = f"{libfile}:{name}"
        if lib_id not in lib_defs:
            base = _extends_of(libfile, name)
            if base:
                # On APLATIT l'extends : KiCad n'accepte pas une base au nom
                # court dans le lib_symbols d'un schéma. On prend le dessin et
                # les pins de la base, renommés au nom du dérivé. Les
                # propriétés (Value, Footprint) viennent de l'instance.
                lib_defs[lib_id] = (sym_block_text(libfile, base), base, name)
            else:
                lib_defs[lib_id] = (sym_block_text(libfile, name), name, name)
        return lib_id

    # --- ICs et connecteurs multi-broches ---
    for ref, spec in CONNECTIONS.items():
        libfile, name = spec["lib"].split(":", 1)
        lib_id = ensure_lib(libfile, name)
        x, y = place()
        body.append(
            emit_instance(ref, lib_id, spec["value"], spec["footprint"], x, y)
        )
        pins = {p[0]: p for p in pin_positions(libfile, name)}
        for num, net in spec["nets"].items():
            if num not in pins:
                raise SystemExit(f"{ref}: broche {num} absente du symbole {name}")
            _, px, py, _ang, _ln = pins[num]
            ex, ey = endpoint(x, y, px, py)
            if net == "":
                body.append(emit_no_connect(ex, ey))
            else:
                body.append(emit_global_label(net, ex, ey))
        # Broches non listées : no_connect (évite les avertissements pin
        # flottante sur les nombreuses IO libres de l'ESP32).
        for num, (_, px, py, _ang, _ln) in pins.items():
            if num not in spec["nets"]:
                ex, ey = endpoint(x, y, px, py)
                body.append(emit_no_connect(ex, ey))

    # --- Composants à deux broches ---
    for ref, value, lib, fp, n1, n2 in TWO_PIN:
        libfile, name = lib.split(":", 1)
        lib_id = ensure_lib(libfile, name)
        x, y = place()
        body.append(emit_instance(ref, lib_id, value, resolve_footprint(ref, fp), x, y))
        pins = {p[0]: p for p in pin_positions(libfile, name)}
        for num, net in (("1", n1), ("2", n2)):
            _, px, py, _ang, _ln = pins[num]
            ex, ey = endpoint(x, y, px, py)
            body.append(emit_global_label(net, ex, ey))

    # --- Boutons ---
    for ref, name, n1, n2 in BUTTONS:
        lib_id = ensure_lib("Switch", name)
        x, y = place()
        body.append(emit_instance(
            ref, lib_id, "SW_Push",
            "Button_Switch_SMD:SW_Push_1TS009xxxx-xxxx-xxxx_6x6x5mm", x, y))
        pins = {p[0]: p for p in pin_positions("Switch", name)}
        # SW_Push a 2 broches nommées 1 et 2.
        for num, net in (("1", n1), ("2", n2)):
            _, px, py, _ang, _ln = pins[num]
            ex, ey = endpoint(x, y, px, py)
            body.append(emit_global_label(net, ex, ey))

    # --- PWR_FLAG : un par rail source ---
    lib_defs["power:PWR_FLAG"] = (sym_block_text_power("PWR_FLAG"), "PWR_FLAG", "PWR_FLAG")
    for net in POWER_FLAGS:
        x, y = place()
        body.append(emit_pwr_flag(net, x, y))

    # --- Assemblage du fichier ---
    lib_symbols_txt = build_lib_symbols(lib_defs)
    out = []
    out.append("(kicad_sch\n")
    out.append("\t(version 20250114)\n")
    out.append('\t(generator "vario-esp32")\n')
    out.append('\t(generator_version "9.0")\n')
    out.append(f'\t(uuid "{SHEET_UUID}")\n')
    out.append('\t(paper "A3")\n')
    out.append(lib_symbols_txt)
    out.extend(body)
    out.append(")\n")
    OUT.write_text("".join(out))
    n = len(CONNECTIONS) + len(TWO_PIN) + len(BUTTONS) + len(POWER_FLAGS)
    print(f"Écrit {OUT} — {n} instances, {len(lib_defs)+1} symboles.")


# --- PWR_FLAG : géré à part car symbole spécial power ---

def emit_pwr_flag(net, x, y):
    ref = uid("flg")
    return (
        f'\t(symbol (lib_id "power:PWR_FLAG") (at {x} {y} 0) (unit 1)\n'
        f"\t\t(exclude_from_sim no) (in_bom no) (on_board yes) (dnp no)\n"
        f'\t\t(uuid "{ref}")\n'
        f'\t\t(property "Reference" "#FLG{_uid_counter[0]}" (at {x} {y-3} 0)\n'
        f"\t\t\t(effects (font (size 1.27 1.27)) (hide yes)))\n"
        f'\t\t(property "Value" "PWR_FLAG" (at {x} {y-5} 0)\n'
        f"\t\t\t(effects (font (size 1.27 1.27))))\n"
        f'\t\t(property "Footprint" "" (at {x} {y} 0)\n'
        f"\t\t\t(effects (font (size 1.27 1.27)) (hide yes)))\n"
        f'\t\t(property "Datasheet" "" (at {x} {y} 0)\n'
        f"\t\t\t(effects (font (size 1.27 1.27)) (hide yes)))\n"
        f'\t\t(instances (project "{PROJECT}"\n'
        f'\t\t\t(path "/{SHEET_UUID}" (reference "#FLG{_uid_counter[0]}") (unit 1)))))\n'
        # La broche du PWR_FLAG est à (0,0) longueur 0 : extrémité = (x, y).
        f'\t(global_label "{net}" (shape input) (at {x} {y} 0)\n'
        f"\t\t(effects (font (size 1.27 1.27)) (justify left))\n"
        f'\t\t(uuid "{uid(net+"flg")}"))\n'
    )


def sym_block_text_power(name):
    """Bloc S-expr du symbole power:<name>."""
    txt = (SYM_DIR / "power.kicad_sym").read_text()
    i = txt.find(f'(symbol "{name}"')
    depth = 0
    for k in range(i, len(txt)):
        if txt[k] == "(":
            depth += 1
        elif txt[k] == ")":
            depth -= 1
            if depth == 0:
                return txt[i : k + 1]
    raise SystemExit(f"symbole power introuvable : {name}")


def build_lib_symbols(lib_defs):
    """Bloc (lib_symbols ...) : chaque symbole renommé à son lib_id.

    Les extends sont déjà aplatis en amont (on prend le bloc de la base et on
    le renomme au lib_id du dérivé), donc aucun symbole ne référence une base.
    """
    parts = ["\t(lib_symbols\n"]
    for lib_id, (block, src, dst) in lib_defs.items():
        # Convention KiCad dans un schéma : le symbole racine porte le lib_id
        # complet "LIB:NOM", mais ses sous-unités gardent le NOM SEUL, non
        # préfixé, sous la forme "NOM_u_v". Quand on aplatit un extends, src
        # (nom de la base) devient dst (nom du dérivé) pour les deux.
        renamed = block.replace(f'(symbol "{src}_', f'(symbol "{dst}_')
        renamed = renamed.replace(f'(symbol "{src}"', f'(symbol "{lib_id}"', 1)
        parts.append(reindent(renamed, 2))
    parts.append("\t)\n")
    return "".join(parts)


def reindent(block, tabs):
    prefix = "\t" * tabs
    return "\n".join(prefix + line if line.strip() else line for line in block.splitlines()) + "\n"


if __name__ == "__main__":
    main()
