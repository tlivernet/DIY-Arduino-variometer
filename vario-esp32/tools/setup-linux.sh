#!/usr/bin/env bash
#
# Installation guidée de l'environnement de développement sous Linux.
#
# Le script AFFICHE ce qu'il va faire et demande confirmation avant chaque
# groupe. Rien n'est installé en silence, et rien n'est désinstallé sans accord
# explicite.
#
#   ./tools/setup-linux.sh              tout, en demandant à chaque étape
#   ./tools/setup-linux.sh --dry-run    affiche seulement les commandes
#   ./tools/setup-linux.sh --yes        ne demande rien (pour un conteneur)

set -euo pipefail

DRY_RUN=0
ASSUME_YES=0
for arg in "$@"; do
  case "$arg" in
    --dry-run) DRY_RUN=1 ;;
    --yes | -y) ASSUME_YES=1 ;;
    *)
      echo "option inconnue : $arg" >&2
      exit 2
      ;;
  esac
done

# --- Détection de la distribution -------------------------------------------

FAMILY="inconnue"
if [ -r /etc/os-release ]; then
  # shellcheck disable=SC1091
  . /etc/os-release
  case "${ID:-}${ID_LIKE:-}" in
    *debian* | *ubuntu*) FAMILY="debian" ;;
    *fedora* | *rhel*) FAMILY="fedora" ;;
    *arch*) FAMILY="arch" ;;
    *suse*) FAMILY="suse" ;;
  esac
fi

echo
echo "Distribution détectée : ${PRETTY_NAME:-inconnue}  (famille : $FAMILY)"

if [ "$FAMILY" = "inconnue" ]; then
  echo
  echo "Famille de distribution non reconnue. Le script ne devinera pas le"
  echo "gestionnaire de paquets ; installe à la main l'équivalent de :"
  echo "  g++, make, git, python3, python3-venv, pipx, clang-format, curl"
  echo "puis relance ./tools/doctor.sh."
  exit 1
fi

# Le groupe des périphériques série n'a pas le même nom partout.
SERIAL_GROUP="dialout"
[ "$FAMILY" = "arch" ] && SERIAL_GROUP="uucp"

# $USER n'est pas défini dans un shell non-login, un conteneur ou une tâche
# cron ; id -un le donne toujours.
TARGET_USER="${USER:-$(id -un)}"

# --- Exécution -------------------------------------------------------------

run() {
  echo "  \$ $*"
  if [ "$DRY_RUN" -eq 0 ]; then
    "$@"
  fi
}

confirm() {
  [ "$ASSUME_YES" -eq 1 ] && return 0
  [ "$DRY_RUN" -eq 1 ] && return 0
  local answer
  read -r -p "  Continuer ? [o/N] " answer
  case "$answer" in
    o | O | y | Y | oui | yes) return 0 ;;
    *)
      echo "  ignoré."
      return 1
      ;;
  esac
}

section() {
  echo
  echo "--- $1 ---"
}

# --- 1. Outils de base ------------------------------------------------------

section "Outils de base (indispensables : tests, rendu des écrans)"
echo "  g++, make, git, python3, pipx, clang-format, curl"
if confirm; then
  case "$FAMILY" in
    debian)
      run sudo apt-get update
      run sudo apt-get install -y build-essential git python3 python3-venv pipx \
        clang-format curl
      ;;
    fedora)
      run sudo dnf install -y gcc-c++ make git python3 pipx clang-tools-extra curl
      ;;
    arch)
      run sudo pacman -S --needed --noconfirm base-devel git python python-pipx clang curl
      ;;
    suse)
      run sudo zypper install -y gcc-c++ make git python3 python3-pipx clang curl
      ;;
  esac
  run pipx ensurepath
fi

# --- 2. PlatformIO ----------------------------------------------------------

section "PlatformIO (compilation et flashage de l'ESP32-S3)"
echo "  Installé par pipx et non par pip : sur Debian et Ubuntu récents, pip"
echo "  refuse d'écrire dans le Python du système (« externally-managed"
echo "  environment »), et pipx isole proprement l'outil."
if confirm; then
  run pipx install platformio
fi

# --- 3. Accès au port série -------------------------------------------------

section "Accès au port série"
echo "  Sans cela le flashage échoue avec « Permission denied »."
echo "  Groupe visé sur cette distribution : $SERIAL_GROUP"
if confirm; then
  run sudo usermod -aG "$SERIAL_GROUP" "$TARGET_USER"

  echo "  Règles udev de PlatformIO :"
  if [ "$DRY_RUN" -eq 0 ]; then
    curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules |
      sudo tee /etc/udev/rules.d/99-platformio-udev.rules >/dev/null
  else
    echo "  \$ curl ... 99-platformio-udev.rules | sudo tee /etc/udev/rules.d/..."
  fi
  run sudo udevadm control --reload-rules
  run sudo udevadm trigger

  echo
  echo "  ATTENTION : l'appartenance à un groupe ne prend effet qu'à la"
  echo "  prochaine ouverture de session. Déconnecte-toi et reconnecte-toi,"
  echo "  ou lance « newgrp $SERIAL_GROUP » dans ce terminal."
fi

# --- 4. Documentation -------------------------------------------------------

section "MkDocs (site de documentation, optionnel)"
if confirm; then
  run pipx install mkdocs
  run pipx inject mkdocs mkdocs-material
fi

# --- 5. Étapes suivantes ----------------------------------------------------

section "KiCad et OpenSCAD (étapes P7 et P8, pas encore nécessaires)"
echo "  Volontairement NON installés automatiquement : ce sont de gros paquets,"
echo "  et les versions des dépôts sont souvent trop anciennes."
echo
echo "  KiCad 9 :"
case "$FAMILY" in
  debian) echo "    sudo add-apt-repository ppa:kicad/kicad-9.0-releases && sudo apt install kicad" ;;
  fedora) echo "    sudo dnf install kicad kicad-packages3d" ;;
  arch) echo "    sudo pacman -S kicad kicad-library kicad-library-3d" ;;
  suse) echo "    sudo zypper install kicad" ;;
esac
echo "    ou, portable et à jour :  flatpak install flathub org.kicad.KiCad"
echo
echo "  OpenSCAD :"
echo "    flatpak install flathub org.openscad.OpenSCAD"
echo "    Passer par Flatpak ou l'AppImage de développement : le moteur"
echo "    « Manifold », beaucoup plus rapide, n'est présent que dans les"
echo "    versions 2024 et suivantes, alors que les dépôts livrent encore"
echo "    souvent la 2021.01."

# --- Bilan ------------------------------------------------------------------

echo
echo "--- Vérification ---"
if [ "$DRY_RUN" -eq 0 ]; then
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  "$script_dir/doctor.sh" || true
else
  echo "  (mode --dry-run : rien n'a été installé)"
fi

echo "Ensuite, depuis vario-esp32/ :"
echo "    make test          les 166 tests du cœur métier"
echo "    make render        les écrans en PNG dans build/screens/"
echo "    pio run -e esp32s3 le firmware — c'est ce qui n'a jamais été compilé"
echo
