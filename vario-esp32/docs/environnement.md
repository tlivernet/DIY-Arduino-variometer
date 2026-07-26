# Environnement de développement

## Le plus court chemin

```bash
git clone https://github.com/tlivernet/DIY-Arduino-variometer.git
cd DIY-Arduino-variometer/vario-esp32

./tools/doctor.sh              # que manque-t-il ?
./tools/setup-linux.sh         # installation guidée, avec confirmation
```

`setup-linux.sh` **affiche chaque commande et demande confirmation** avant chaque groupe. Rien n'est installé en silence, rien n'est désinstallé. Pour voir ce qu'il ferait sans rien toucher :

```bash
./tools/setup-linux.sh --dry-run
```

Il reconnaît les familles Debian/Ubuntu, Fedora/RHEL, Arch et openSUSE. Sur une autre distribution il refuse de deviner et affiche la liste des paquets à installer à la main.

## Ce qu'il faut, par usage

### Pour lancer les tests : trois paquets

C'est volontaire. Le cœur métier est du C++17 sans aucune dépendance, et Unity est inclus dans le dépôt.

```bash
sudo apt install build-essential git python3     # Debian / Ubuntu
```

```bash
make test      # 166 tests
make render    # les écrans en PNG dans build/screens/
```

`make render` n'a besoin de rien de plus : la conversion PBM → PNG est faite en Python pur, `zlib` étant dans la bibliothèque standard.

### Pour compiler et flasher : PlatformIO

```bash
sudo apt install pipx
pipx ensurepath
pipx install platformio
```

!!! warning "pipx et non pip"
    Sur Debian et Ubuntu récents, `pip install platformio` échoue avec « externally-managed-environment » : pip refuse d'écrire dans le Python du système. `pipx` isole l'outil dans son propre environnement et met `pio` dans le PATH.

Puis :

```bash
pio run -e esp32s3            # compiler
pio run -e esp32s3 -t upload  # flasher
pio device monitor            # console série à 115200
```

### Accès au port série — l'oubli classique

Sans cela le flashage échoue avec « Permission denied » et on cherche longtemps.

```bash
sudo usermod -aG dialout "$(id -un)"      # uucp sur Arch

curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules \
  | sudo tee /etc/udev/rules.d/99-platformio-udev.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```

!!! danger "Il faut se reconnecter"
    L'appartenance à un groupe ne prend effet qu'à la prochaine ouverture de session. Dans l'immédiat, `newgrp dialout` suffit pour le terminal courant.

!!! tip "Piège Ubuntu : brltty"
    Sur Ubuntu, le paquet `brltty` — support des afficheurs braille — **capture les puces série CH340 et CP210x**. Symptôme : la carte apparaît puis disparaît quelques secondes après avoir été branchée. Sans effet sur une carte à USB natif comme l'ESP32-S3-Zero, mais impossible à diagnostiquer si on ne le sait pas. Remède : `sudo apt remove brltty`.

### Pour les vérifications de la CI

```bash
sudo apt install clang-format
```

Le contrôle de style est bloquant en intégration continue, donc autant l'avoir en local :

```bash
make hal-syntax    # syntaxe de la couche matérielle contre des talons
make bom           # valide les nomenclatures et engendre les formats
```

### Pour la documentation

```bash
pipx install mkdocs
pipx inject mkdocs mkdocs-material

mkdocs serve       # aperçu sur http://127.0.0.1:8000
```

### Pour les étapes P7 et P8

Ni KiCad ni OpenSCAD ne sont installés automatiquement : ce sont de gros paquets, et les versions des dépôts sont souvent trop anciennes.

**KiCad 9** — `flatpak install flathub org.kicad.KiCad` est le plus portable et le plus à jour. Sinon le PPA `ppa:kicad/kicad-9.0-releases` sur Ubuntu, ou le paquet de la distribution.

**OpenSCAD** — passer par `flatpak install flathub org.openscad.OpenSCAD` ou l'AppImage de développement. Le moteur **Manifold**, beaucoup plus rapide, n'existe que dans les versions 2024 et suivantes, alors que les dépôts livrent encore souvent la 2021.01.

## Récapitulatif

| Outil | Nécessaire pour | Indispensable ? |
|---|---|---|
| `g++`, `make`, `git` | tests, rendu des écrans | oui |
| `python3` | images, nomenclatures | oui |
| `pio` (PlatformIO) | compiler et flasher | oui, dès qu'il y a du matériel |
| groupe `dialout` + règles udev | flasher | oui, dès qu'il y a du matériel |
| `clang-format` | contrôle de style bloquant en CI | recommandé |
| `mkdocs` + `mkdocs-material` | site de documentation | optionnel |
| KiCad | circuit imprimé (P7) | pas encore |
| OpenSCAD | boîtier (P8) | pas encore |

## Éditeur

Sans importance, mais deux choses aident :

- l'extension **PlatformIO IDE** pour VS Code apporte la complétion sur les en-têtes de l'ESP-IDF ;
- `pio run -t compiledb` engendre un `compile_commands.json`, que `clangd` utilise pour donner une complétion correcte dans n'importe quel éditeur.
