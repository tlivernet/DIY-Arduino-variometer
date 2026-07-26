"""Mini-lecteur de S-expressions KiCad — autonome, sans dépendance externe.

Les générateurs de schéma et de PCB n'ont besoin que de lire des fichiers
S-expression (bibliothèques de symboles, netlist). Ce module évite de dépendre
d'un greffon installé sur une machine particulière : le dépôt reste
reconstructible tel quel.

    doc = parse_file("fichier.kicad_sym")   # -> listes/chaînes imbriquées
    at = find_first(node, "at")             # premier sous-nœud (at ...)
"""


def parse(text):
    """Analyse une chaîne S-expression en listes Python imbriquées.

    Les atomes sont des chaînes ; les chaînes entre guillemets sont
    déséchappées. Retourne la liste des expressions de premier niveau.
    """
    tokens = _tokenize(text)
    pos = 0
    out = []
    while pos < len(tokens):
        node, pos = _read(tokens, pos)
        out.append(node)
    # La plupart des fichiers ont une seule expression racine ; on renvoie son
    # contenu aplati pour rester compatible avec l'usage attendu (itérer sur
    # les sous-nœuds du niveau racine).
    if len(out) == 1 and isinstance(out[0], list):
        return out[0]
    return out


def parse_file(path):
    with open(path, encoding="utf-8") as f:
        return parse(f.read())


def find_first(node, keyword):
    """Premier sous-nœud liste dont le premier élément est `keyword`."""
    if not isinstance(node, list):
        return None
    for x in node:
        if isinstance(x, list) and x and x[0] == keyword:
            return x
    return None


def _tokenize(text):
    tokens = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c in " \t\r\n":
            i += 1
        elif c in "()":
            tokens.append(c)
            i += 1
        elif c == '"':
            j = i + 1
            buf = []
            while j < n and text[j] != '"':
                if text[j] == "\\" and j + 1 < n:
                    buf.append(text[j + 1])
                    j += 2
                else:
                    buf.append(text[j])
                    j += 1
            tokens.append(('"', "".join(buf)))
            i = j + 1
        else:
            j = i
            while j < n and text[j] not in " \t\r\n()":
                j += 1
            tokens.append(text[i:j])
            i = j
    return tokens


def _read(tokens, pos):
    tok = tokens[pos]
    if tok == "(":
        pos += 1
        lst = []
        while tokens[pos] != ")":
            node, pos = _read(tokens, pos)
            lst.append(node)
        return lst, pos + 1
    if isinstance(tok, tuple):  # chaîne entre guillemets
        return tok[1], pos + 1
    return tok, pos + 1
