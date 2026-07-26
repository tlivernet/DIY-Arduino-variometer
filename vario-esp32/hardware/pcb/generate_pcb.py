#!/usr/bin/env python3
"""Engendre le circuit imprimé (placement + contour + plans de masse) — P7 suite.

Deuxième étage du pipeline PCB, après le schéma. Lit la netlist exportée du
schéma (source de vérité déjà validée par l'ERC), charge chaque empreinte,
assigne les nets, place les composants par BLOCS FONCTIONNELS sur un contour de
carte au format paysage (dicté par l'écran), et pose deux plans de masse.

    kicad-cli sch export netlist vario.kicad_sch -o vario.net   # entrée
    python3 generate_pcb.py                                     # -> vario.kicad_pcb

Le PLACEMENT est raisonné mais grossier ; le ROUTAGE est fait ensuite par
freerouting (voir route_pcb.py). La qualité fine (masse, antenne, USB) demande
une relecture humaine — voir docs/pcb.md.

Nécessite l'API Python de KiCad (`import pcbnew`), fournie par KiCad 9.
"""

from pathlib import Path

import pcbnew

import sexp as sp

HERE = Path(__file__).resolve().parent
NETLIST = Path("/tmp/vario.net")
OUT = HERE / "vario.kicad_pcb"
FP_DIR = Path("/usr/share/kicad/footprints")

# --- Contour de carte -------------------------------------------------------
# L'écran LS027B7DH01 (~60 × 43 mm, format paysage, nappe FPC sortant au bas
# d'un long côté) dicte l'encombrement. On vise une carte un peu plus grande.
BOARD_W = 78.0   # largeur (X)
BOARD_H = 58.0   # hauteur (Y)
MARGIN = 3.0     # marge intérieure de placement (edge clearance confortable)

# --- Placement par blocs ----------------------------------------------------
# Chaque bloc regroupe des repères autour d'une ancre (x, y) en mm depuis le
# coin haut-gauche. Le placement est grossier : freerouting route ensuite, et
# l'humain resserre. Objectif : regrouper ce qui doit être proche (découplages
# près de leur IC, capteurs sur le bus I2C, alims ensemble).
# Carte 78 × 58 mm. Repère : X vers la droite, Y vers le bas, origine coin
# haut-gauche. L'ESP32 (module 25,5 × 18 mm, mais courtyard 48 × 41 à cause du
# keep-out antenne) est collé en HAUT-GAUCHE, antenne vers le bord haut-gauche :
# son keep-out déborde hors carte et ne gêne personne.
BLOCKS = {
    # Calculateur, module au bord haut-gauche (antenne dégagée vers l'extérieur),
    # décalé à droite pour dégager l'accu et son connecteur sur le bord gauche.
    "U1": (24, 14),
    # Capteurs alignés à droite du module, bus I2C court, en haut à droite.
    "U2": (52, 8), "U3": (60, 8), "U4": (70, 10),
    "R1": (52, 16), "R2": (56, 16), "R15": (48, 12), "R16": (64, 12),
    # Alimentation : chargeur, boost, LDO, inductance — à droite au milieu.
    "U5": (72, 22), "U6": (60, 24), "U7": (72, 28),
    "L1": (60, 30), "D1": (74, 18),
    "R5": (68, 22), "R10": (54, 30), "R11": (54, 34),
    "R3": (72, 34), "R4": (76, 34),
    # Audio : buffers, au dos, décalés du coin gauche encombré par le support
    # de pile ; piézo en bas-gauche.
    "U8": (24, 30), "U9": (24, 34), "LS1": (50, 53),
    # Boutons de façade (courtyard ~17 mm) : espacés de 20 mm, remontés à y=44
    # pour laisser le bord bas à la nappe écran.
    "SW1": (30, 40), "SW2": (48, 40), "SW3": (66, 40),
    "R8": (34, 34), "R9": (58, 36),
    # Connecteurs : USB au bord droit (reculé pour ne pas déborder), écran FPC
    # au tout bord bas (nappe paysage sortant vers le bas), accu à gauche.
    # Support de pile CR1220 (Ø 12,6 mm) : bien à l'intérieur du bord gauche.
    "J1": (70, 54), "J2": (8, 48), "J3": (30, 55),
    "BAT2": (13, 24),
    # Reset ESP32.
    "R12": (36, 24), "C15": (40, 24),
    # Découplages : au dos, répartis près de leurs rails.
    "C1": (16, 24), "C2": (24, 24), "C3": (30, 12), "C4": (36, 16),
    "C5": (64, 18), "C6": (68, 14), "C7": (14, 28), "C8": (64, 30),
    "C9": (58, 20), "C10": (68, 40), "C11": (54, 22), "C12": (58, 34),
    "C13": (76, 30), "C14": (24, 28),
    "R6": (70, 46), "R7": (76, 46),   # CC USB
    "R13": (66, 26), "R14": (70, 16),  # tirages divers
}

# Repères à poser en face ARRIÈRE (B.Cu) pour dégager la face avant (écran).
# Les découplages et petits passifs vont au dos ; l'écran occupe toute l'avant.
BACK_SIDE = {f"C{i}" for i in range(1, 16)} | {
    "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "R9",
    "R10", "R11", "R12", "R13", "R14", "R15", "R16", "L1", "D1",
    "U5", "U6", "U7", "U8", "U9",
}


def parse_netlist(path):
    """Retourne (components, nets).

    components: [{ref, footprint, value}]
    nets: {netname: [(ref, pinnum), ...]}
    """
    doc = sp.parse_file(str(path))
    comps, nets_raw = [], []

    def walk(n):
        if isinstance(n, list):
            if n and n[0] == "comp":
                comps.append(n)
            if n and n[0] == "net":
                nets_raw.append(n)
            for x in n:
                walk(x)

    walk(doc)

    components = []
    for c in comps:
        ref = _val(c, "ref")
        fp = _val(c, "footprint")
        value = _val(c, "value")
        components.append({"ref": ref, "footprint": fp, "value": value})

    nets = {}
    for net in nets_raw:
        name = _val(net, "name")
        nodes = []
        for x in net:
            if isinstance(x, list) and x and x[0] == "node":
                nodes.append((_val(x, "ref"), _val(x, "pin")))
        nets[name] = nodes
    return components, nets


def _val(node, key):
    for x in node:
        if isinstance(x, list) and x and x[0] == key and len(x) > 1:
            return x[1]
    return None


def add_board_outline(board):
    """Rectangle de contour sur Edge.Cuts."""
    pts = [(0, 0), (BOARD_W, 0), (BOARD_W, BOARD_H), (0, BOARD_H), (0, 0)]
    for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
        seg = pcbnew.PCB_SHAPE(board)
        seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
        seg.SetStart(pcbnew.VECTOR2I(pcbnew.FromMM(x1), pcbnew.FromMM(y1)))
        seg.SetEnd(pcbnew.VECTOR2I(pcbnew.FromMM(x2), pcbnew.FromMM(y2)))
        seg.SetLayer(pcbnew.Edge_Cuts)
        seg.SetWidth(pcbnew.FromMM(0.1))
        board.Add(seg)


def add_ground_zone(board, gnd_net, layer):
    """Plan de masse couvrant toute la carte sur une couche."""
    zone = pcbnew.ZONE(board)
    zone.SetLayer(layer)
    zone.SetNet(gnd_net)
    zone.SetIsFilled(True)
    outline = zone.Outline()
    outline.NewOutline()
    m = 0.3
    for x, y in [(m, m), (BOARD_W - m, m), (BOARD_W - m, BOARD_H - m), (m, BOARD_H - m)]:
        outline.Append(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
    board.Add(zone)
    return zone


def add_stitching_vias(board, gnd_net, step=4.0):
    """Grille de vias GND cousant les deux plans de masse.

    Sans couture, le plan GND se fragmente en îlots isolés par les pistes, et
    des pads GND se retrouvent « non connectés ». Une grille de vias reliant
    F.Cu et B.Cu garantit un plan continu. Les vias tombant sous une empreinte
    (courtyard) sont omis pour éviter les collisions.
    """
    boxes = []
    for fp in board.GetFootprints():
        x, y, w, h = _courtyard_bbox_mm(fp)
        boxes.append((x - 0.5, y - 0.5, x + w + 0.5, y + h + 0.5))

    def under_footprint(px, py):
        return any(x0 <= px <= x1 and y0 <= py <= y1 for x0, y0, x1, y1 in boxes)

    def place_via(px, py):
        via = pcbnew.PCB_VIA(board)
        via.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(px), pcbnew.FromMM(py)))
        via.SetDrill(pcbnew.FromMM(0.3))
        via.SetWidth(pcbnew.FromMM(0.6))
        via.SetNet(gnd_net)
        via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        board.Add(via)

    n = 0
    # Grille de fond : couture générale des deux plans.
    y = step
    while y < BOARD_H - step / 2:
        x = step
        while x < BOARD_W - step / 2:
            if not under_footprint(x, y):
                place_via(x, y)
                n += 1
            x += step
        y += step

    # Couture locale : un via GND à côté de CHAQUE pad GND, pour garantir que la
    # région de plan autour de ce pad est reliée aux deux couches (c'est là que
    # les îlots isolent sinon un pad). Décalé de 1,3 mm dans la première
    # direction qui ne tombe ni sous une empreinte ni sur le pad.
    for fp in board.GetFootprints():
        for pad in fp.Pads():
            if pad.GetNetname() != "GND":
                continue
            px = pcbnew.ToMM(pad.GetPosition().x)
            py = pcbnew.ToMM(pad.GetPosition().y)
            for dx, dy in [(1.3, 0), (-1.3, 0), (0, 1.3), (0, -1.3),
                           (1.0, 1.0), (-1.0, -1.0)]:
                vx, vy = px + dx, py + dy
                if 1 < vx < BOARD_W - 1 and 1 < vy < BOARD_H - 1 \
                        and not under_footprint(vx, vy):
                    place_via(vx, vy)
                    n += 1
                    break
    return n


def find_footprint_file(fp_id):
    """Chemin du .kicad_mod pour un id 'Lib:Nom'."""
    lib, mod = fp_id.split(":", 1)
    return FP_DIR / f"{lib}.pretty", mod


# Empreintes dont la position est IMPOSÉE (ancrées) : on ne les déplace pas
# lors du désempilement, ce sont elles qui définissent l'ergonomie et les bords.
# J1 (USB) et les alims (U5-U7) NE sont PAS ancrés : on les laisse l'algo
# écarter des boutons/capteurs. Restent ancrés : le module, la façade (boutons,
# écran, piézo), l'accu — leur position définit l'ergonomie.
ANCHORED = {"U1", "J2", "J3", "BAT2", "SW1", "SW2", "SW3", "LS1",
            "U2", "U3", "U4"}


def _courtyard_bbox_mm(fp):
    """Boîte englobante du courtyard avant, en mm (x, y, w, h)."""
    cy = fp.GetCourtyard(pcbnew.F_CrtYd)
    bb = cy.BBox() if cy.OutlineCount() else fp.GetBoundingBox(False, False)
    return (
        pcbnew.ToMM(bb.GetX()), pcbnew.ToMM(bb.GetY()),
        pcbnew.ToMM(bb.GetWidth()), pcbnew.ToMM(bb.GetHeight()),
    )


def separate_overlaps(board, passes=40, pad=0.4):
    """Écarte itérativement les empreintes dont les courtyards se chevauchent.

    Approche « force de répulsion » simple : à chaque passe, pour chaque paire
    qui se recouvre, on pousse la MOINS ancrée (ou les deux si aucune n'est
    ancrée) le long du plus petit axe de chevauchement. Le keep-out antenne de
    l'ESP32 est ignoré : il déborde exprès hors carte, on ne désempile pas U1.
    """
    fps = [fp for fp in board.GetFootprints()]
    for _ in range(passes):
        moved = False
        for i, a in enumerate(fps):
            for b in fps[i + 1:]:
                ra, rb = a.GetReference(), b.GetReference()
                if "U1" in (ra, rb):
                    continue  # keep-out antenne : ne pas désempiler
                ax, ay, aw, ah = _courtyard_bbox_mm(a)
                bx, by, bw, bh = _courtyard_bbox_mm(b)
                # Chevauchement sur chaque axe (avec marge pad).
                ox = min(ax + aw, bx + bw) - max(ax, bx) + pad
                oy = min(ay + ah, by + bh) - max(ay, by) + pad
                if ox <= 0 or oy <= 0:
                    continue  # pas de recouvrement
                moved = True
                # Pousser le long de l'axe de moindre recouvrement.
                if ox < oy:
                    d = ox / 2 if _both_free(ra, rb) else ox
                    sign = 1 if (ax + aw / 2) <= (bx + bw / 2) else -1
                    _shift(a, b, ra, rb, dx=sign * d)
                else:
                    d = oy / 2 if _both_free(ra, rb) else oy
                    sign = 1 if (ay + ah / 2) <= (by + bh / 2) else -1
                    _shift(a, b, ra, rb, dy=sign * d)
        if not moved:
            break


def _both_free(ra, rb):
    return ra not in ANCHORED and rb not in ANCHORED


def _shift(a, b, ra, rb, dx=0.0, dy=0.0):
    """Décale la ou les empreintes libres. Si l'une est ancrée, ne bouge que
    l'autre (dans le sens opposé)."""
    def move(fp, mx, my):
        p = fp.GetPosition()
        fp.SetPosition(pcbnew.VECTOR2I(
            p.x + pcbnew.FromMM(mx), p.y + pcbnew.FromMM(my)))

    a_free = ra not in ANCHORED
    b_free = rb not in ANCHORED
    if a_free and b_free:
        move(a, -dx, -dy)
        move(b, dx, dy)
    elif b_free:
        move(b, dx, dy)
    elif a_free:
        move(a, -dx, -dy)


def main():
    if not NETLIST.exists():
        raise SystemExit(
            f"netlist absente : {NETLIST}\n"
            "Lancer d'abord : kicad-cli sch export netlist vario.kicad_sch "
            f"-o {NETLIST}"
        )
    components, nets = parse_netlist(NETLIST)

    board = pcbnew.BOARD()

    # Nets : créer chaque net nommé.
    net_by_name = {}
    for name in nets:
        ni = pcbnew.NETINFO_ITEM(board, name)
        board.Add(ni)
        net_by_name[name] = ni

    # Index pin -> net pour assignation rapide.
    pin_net = {}  # (ref, pin) -> netname
    for name, nodes in nets.items():
        for ref, pin in nodes:
            pin_net[(ref, pin)] = name

    # Charger et placer chaque empreinte.
    placed = 0
    for comp in components:
        ref = comp["ref"]
        fp_id = comp["footprint"]
        if not fp_id or ":" not in fp_id:
            print(f"  ! {ref}: empreinte manquante/invalide ({fp_id}), ignoré")
            continue
        lib_dir, mod = find_footprint_file(fp_id)
        fp = pcbnew.FootprintLoad(str(lib_dir), mod)
        if fp is None:
            print(f"  ! {ref}: empreinte introuvable {fp_id}")
            continue
        fp.SetReference(ref)
        fp.SetValue(comp["value"] or "")
        board.Add(fp)

        # Position (ancre du bloc), sinon coin haut-gauche décalé.
        x, y = BLOCKS.get(ref, (MARGIN + placed % 10 * 5, MARGIN + placed // 10 * 5))
        fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))

        # Face arrière si listé.
        if ref in BACK_SIDE:
            fp.Flip(fp.GetPosition(), False)

        # Assigner les nets aux pads.
        for pad in fp.Pads():
            key = (ref, pad.GetNumber())
            name = pin_net.get(key)
            if name:
                pad.SetNet(net_by_name[name])
        placed += 1

    separate_overlaps(board)
    add_board_outline(board)
    if net_by_name.get("GND"):
        add_stitching_vias(board, net_by_name["GND"])

    gnd = net_by_name.get("GND")
    if gnd:
        add_ground_zone(board, gnd, pcbnew.F_Cu)
        add_ground_zone(board, gnd, pcbnew.B_Cu)

    board.Save(str(OUT))
    print(f"Écrit {OUT} — {placed} empreintes placées, "
          f"{len(nets)} nets, contour {BOARD_W}×{BOARD_H} mm, 2 plans de masse.")


if __name__ == "__main__":
    main()
