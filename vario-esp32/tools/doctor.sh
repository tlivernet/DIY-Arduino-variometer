#!/usr/bin/env bash
#
# Dresse l'état de l'environnement de développement et dit ce qui manque pour
# quelle étape. Ne modifie rien.
#
#   ./tools/doctor.sh

set -uo pipefail

ok=0
missing_required=0
missing_optional=0

green() { printf '\033[32m%s\033[0m' "$1"; }
red() { printf '\033[31m%s\033[0m' "$1"; }
yellow() { printf '\033[33m%s\033[0m' "$1"; }

# check <commande> <requis|optionnel> <à quoi ça sert> [commande de version]
check() {
  local cmd="$1" level="$2" purpose="$3" version_cmd="${4:-}"

  if command -v "$cmd" >/dev/null 2>&1; then
    local version=""
    if [ -n "$version_cmd" ]; then
      version=$(eval "$version_cmd" 2>/dev/null | head -1 | cut -c1-48)
    fi
    printf '  %s %-14s %s\n' "$(green '[ok]')" "$cmd" "$version"
    ok=$((ok + 1))
  elif [ "$level" = "requis" ]; then
    printf '  %s %-14s %s\n' "$(red '[MANQUE]')" "$cmd" "$purpose"
    missing_required=$((missing_required + 1))
  else
    printf '  %s %-14s %s\n' "$(yellow '[absent]')" "$cmd" "$purpose"
    missing_optional=$((missing_optional + 1))
  fi
}

echo
echo "=== Indispensable : lancer les tests et rendre les écrans ==="
check g++ requis "compilation du cœur métier" "g++ --version"
check make requis "lanceur de tests" "make --version"
check git requis "gestion de version" "git --version"
check python3 requis "conversion des images, validation des nomenclatures" "python3 --version"

echo
echo "=== Compiler et flasher le firmware ==="
check pio requis "PlatformIO — compilation ESP32-S3" "pio --version"

echo
echo "=== Confort et vérifications de la CI ==="
check clang-format optionnel "contrôle de style, imposé par la CI" "clang-format --version"
check clang-tidy optionnel "analyse statique" "clang-tidy --version"

echo
echo "=== Étapes suivantes du projet ==="
check mkdocs optionnel "site de documentation" "mkdocs --version"
check kicad optionnel "circuit imprimé (étape P7)" "kicad-cli version"
check openscad optionnel "boîtier imprimé en 3D (étape P8)" "openscad --version"

echo
echo "=== Accès au port série ==="
target_user="${USER:-$(id -un 2>/dev/null || true)}"
if [ -n "$target_user" ] && id -nG "$target_user" 2>/dev/null | tr ' ' '\n' | grep -qxE 'dialout|uucp'; then
  printf '  %s appartenance au groupe série (dialout/uucp)\n' "$(green '[ok]')"
else
  printf '  %s pas dans le groupe dialout/uucp — le flashage échouera\n' "$(yellow '[absent]')"
  printf '        sudo usermod -aG dialout "$(id -un)"   puis SE RECONNECTER\n'
  missing_optional=$((missing_optional + 1))
fi

if [ -e /etc/udev/rules.d/99-platformio-udev.rules ]; then
  printf '  %s règles udev PlatformIO installées\n' "$(green '[ok]')"
else
  printf '  %s règles udev PlatformIO absentes\n' "$(yellow '[absent]')"
  missing_optional=$((missing_optional + 1))
fi

# Piège classique sur Ubuntu : brltty capture les puces série CH340 et CP210x,
# ce qui fait disparaître la carte du système quelques secondes après l'avoir
# branchée. Sans incidence sur une carte à USB natif, mais impossible à
# diagnostiquer si on ne le sait pas.
if command -v dpkg >/dev/null 2>&1 && dpkg -s brltty >/dev/null 2>&1; then
  printf '  %s brltty est installé : il capture les puces CH340 et CP210x\n' "$(yellow '[!]')"
  printf '        sans effet sur une carte à USB natif, sinon : sudo apt remove brltty\n'
fi

echo
if [ "$missing_required" -eq 0 ]; then
  printf '%s %d outils présents' "$(green 'Prêt.')" "$ok"
else
  printf '%s %d outil(s) indispensable(s) manquant(s)' "$(red 'Incomplet :')" "$missing_required"
fi
[ "$missing_optional" -gt 0 ] && printf ', %d optionnel(s) absent(s)' "$missing_optional"
echo "."
echo
echo "Installation guidée : ./tools/setup-linux.sh"
echo

exit 0
