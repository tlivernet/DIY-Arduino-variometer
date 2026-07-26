# Développement

## Compiler et tester

```bash
# Tests du cœur métier — c'est ce que vérifie l'intégration continue
make test

# Une seule suite
make test-test_kalman

# Rendre les écrans dans build/screens/*.png
make render

# Firmware embarqué (nécessite PlatformIO)
pio run -e esp32s3
```

`make test` n'a besoin que de `g++` et `make` : Unity est inclus dans le dépôt sous licence MIT, donc la suite de tests ne dépend d'aucun téléchargement et fonctionne hors ligne. `pio test -e native` fait la même chose via PlatformIO.

C'est la réponse directe au défaut majeur du projet de 2017, qui ne compilait plus que sur un IDE Arduino de 2015.

## Le principe : cœur pur, périphérie mince

Tout le domaine métier est du **C++17 standard sans aucune dépendance Arduino ou ESP32**, donc compilable et testable sur un PC. Le matériel n'est touché que par des adaptateurs minces qui injectent horloge et mesures dans ce cœur.

```
src/
  core/       C++ pur, entièrement testable en natif
    isa            conversion pression → altitude, QNH borné
    vario_filter   lissage pression + dérivée, indépendant de la cadence
    attitude       estimation d'assiette, compensation centrifuge
    kalman         filtre à 3 états
    audio_gate     inhibition du son au démarrage
    tone_law       lois hauteur/durée des bips
    tone_scheduler machine à états du bipeur
    flight_fsm     détection décollage / atterrissage
    stats          statistiques de vol
    flight_log     journal en ajout seul, 64 octets + CRC
    config_schema  table de schéma des réglages
    lk8ex1         trames pour XCTrack
    buttons        machine à états des trois boutons
    battery, format, vec3, clock
  ui/         couche d'affichage, également pure
    canvas         tampon 1 bit, primitives, lignes modifiées
    segment        chiffres à sept segments
    font5x7        police des petits libellés
    screens        les quatre écrans
  hal/        adaptateurs matériel (à venir)
test/
  test_*/     une suite par module
  support/    Unity, générateurs synthétiques, états de référence
  golden/     images de référence des écrans
tools/
  render_screens.cpp   rend les écrans en PBM
  make_golden.cpp      régénère les images de référence
  pbm2png.py           conversion PBM → PNG, sans dépendance
```

Cette séparation n'est pas cosmétique : c'est elle qui permet de valider le filtre de Kalman sur des trajectoires synthétiques et les écrans sur des images de référence, **sans matériel**.

## Tester sans matériel

### Trajectoires synthétiques

Les générateurs produisent accéléromètre, gyromètre et pression dérivés d'une **même vérité terrain**, exactement comme le feraient de vrais capteurs. Le jeu décisif est le virage coordonné établi : vitesse verticale vraie nulle, mais facteur de charge de g/cos(φ). Un filtre qui se comporte bien là se comportera bien en vol, aux imperfections des capteurs près.

La cohérence des générateurs est elle-même vérifiée : avec une assiette parfaite, l'accélération verticale calculée doit valoir zéro à 10⁻⁶ près.

### Images de référence

Les écrans sont rendus puis comparés octet par octet à des images versionnées. En cas d'écart, l'image obtenue est écrite dans `build/` pour être ouverte.

Après un changement de mise en page **assumé et regardé** :

```bash
make golden
```

À ne jamais lancer pour faire taire un test sans avoir examiné l'image obtenue.

## Intégration continue

Trois tâches, à chaque modification :

| Tâche | Contenu |
|---|---|
| **Tests natifs** | `make test`, puis `make render` avec publication des PNG en artefacts |
| **Firmware** | `pio run -e esp32s3`, avec publication du binaire |
| **Style** | `clang-format --dry-run --Werror` |

Publier les écrans rendus signifie qu'une régression de mise en page se constate à l'œil dans les artefacts, sans rien installer.

## Conventions

**`-Wall -Wextra -Werror`** partout. Cela a déjà attrapé un risque réel de troncature de chaîne pendant l'écriture des écrans.

**`-fno-fast-math -fno-finite-math-only`**, explicitement. `-ffast-math` implique `-ffinite-math-only`, qui autorise le compilateur à considérer `isnan()` et `isfinite()` comme toujours faux et donc à **supprimer exactement les gardes** qui protègent du défaut n°3 de 2017.

**Temps monotone en `int64_t` microsecondes.** L'ancien code stockait `micros()` dans un `float`, dont la mantisse de 24 bits ne représentait plus les microsecondes exactement au-delà de 17 s d'uptime, et le comparait à travers le débordement des 71,6 minutes.

**Un test par défaut historique.** Chacun des dix défauts de 2017 a un test nommé qui le décrit dans son commentaire. Ce sont des pierres tombales : le lecteur comprend pourquoi le code est écrit ainsi.

**Documenter les décisions, pas le code.** Les commentaires expliquent *pourquoi* — pourquoi les sept segments plutôt qu'une police, pourquoi le suivi des lignes modifiées est obligatoire, pourquoi ignorer la température est correct.

## Structure de la mémoire flash

Pas de partition de mise à jour à distance : on flashe par USB natif, et la place ainsi gagnée va au journal de vol et aux fichiers de l'interface web. Une carte de 8 ou 16 Mo permettrait d'avoir les deux.

## État d'avancement

Ce qui a **réellement été exécuté et vérifié** : les 166 tests natifs et le rendu des écrans.

Ce qui n'a **pas** été vérifié : la compilation du firmware pour l'ESP32-S3. Le registre PlatformIO n'était pas joignable depuis l'environnement de développement utilisé, la tâche d'intégration continue en sera le premier test réel. Aucun code matériel n'existe encore, et rien n'a volé.
