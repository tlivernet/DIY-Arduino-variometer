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

### Harnais de rejeu

C'est le meilleur investissement du projet pour un appareil dont la justesse se
définit par une physique qu'on ne peut pas reproduire sur un bureau. Le principe :
le firmware émet les trames brutes — horodatage, accéléromètre, gyromètre,
pression, température — sur l'USB à ~100 Hz (≈ 4 ko/s, rien pour de l'USB natif),
un script les enregistre en CSV, et le rejoue les fait passer **à travers toute
la chaîne d'estimation** en natif.

Conséquence : chaque comportement bizarre constaté en vol devient un
enregistrement de 30 s, qui devient un fichier de référence versionné, qui
devient un test de non-régression permanent. Le réglage du filtre se fait alors
sur dix vrais vols en une seconde sur un portable.

**Enregistrer un vol.** On branche la carte, on lance le firmware, puis :

```bash
tools/record.py --port /dev/ttyACM0 --seconds 30 -o traces/vol.csv
```

Le script envoie la commande `r` au firmware (qui bascule alors en émission de
trames brutes) et enregistre le flux. `pyserial` est la seule dépendance, et
seulement pour cette acquisition.

**Rejouer.** Le rejoue est du C++17 pur, sans dépendance :

```bash
make replay TRACE=traces/vol.csv
```

Il écrit la trace estimée (altitude, vitesse verticale, son) sur la sortie
standard. Ordre des opérations **identique** à `VarioApp::sensorLoop()` : toute
divergence entre le rejoue et le firmware est un bug à corriger, pas une liberté.

**Tests.** `test/test_replay` couvre l'aller-retour CSV du format de trame et
rejoue des vols synthétiques cohérents — dont le critère phare, le **silence au
repos** : trente secondes posé sur une table, aucun son et vario affiché sous
0,05 m/s. C'est le test que le montage de 2017 échouait.

### Portail de configuration (P6)

Le portail Wi-Fi remplace l'édition à l'encodeur pour les réglages fins : on
ouvre le point d'accès `Variometre`, on s'y connecte avec un téléphone, et une
page unique liste tous les réglages.

Le principe est le même que pour le menu embarqué : **la table de schéma
(`config_schema`) est la source unique**. La page ne code aucun réglage en dur —
au chargement elle interroge `GET /config`, reçoit le schéma et les valeurs en
JSON (`configToJson`) et se dessine elle-même ; l'enregistrement renvoie un
`POST /config` que le firmware applique champ par champ via `setConfigValue`,
donc borné (`applyJsonToConfig`). Ajouter un réglage reste une seule ligne dans
`kFields`.

Tout cela est du cœur pur, testé sans matériel (`test/test_config_json`) :
sérialisation fidèle, application bornée, tolérance aux clés inconnues (un client
plus récent ne casse pas un firmware plus ancien), aller-retour stable, et page
autonome (aucune ressource distante). Le transport — point d'accès, serveur HTTP,
persistance NVS — n'est que la fine couche `hal/` autour.

Côté firmware, une **tâche radio sur le cœur 0** gère le portail et le BLE, à
l'écart du temps réel du son : ouvrir le portail ne fait pas hoqueter les bips.
Les réglages sont relus de la flash au démarrage (`hal/config_store`, via NVS,
clé par clé — une clé absente rend son défaut, donc aucune migration à écrire) et
réécrits à chaque enregistrement.

### BLE vers XCTrack (P6)

L'appareil expose le **Nordic UART Service** et y pousse une trame **LK8EX1** par
seconde (`hal/ble_uart`). XCTrack, XCSoar et LK8000 s'en servent alors comme
capteur de pression et de vitesse verticale. On envoie la pression **brute** et
on laisse l'altitude au téléphone : un seul QNH, pas de deux altitudes
divergentes. Le formatage de la trame (`core/lk8ex1`) est testé depuis P3.

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

Ce qui a **réellement été exécuté et vérifié** : les 172 tests natifs (dont le harnais de rejeu), le rejeu d'une trace de démonstration et le rendu des écrans.

Ce qui n'a **pas** été vérifié : la compilation du firmware pour l'ESP32-S3. Le registre PlatformIO n'était pas joignable depuis l'environnement de développement utilisé, la tâche d'intégration continue en sera le premier test réel. Aucun code matériel n'existe encore, et rien n'a volé.
