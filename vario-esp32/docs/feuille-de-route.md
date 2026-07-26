# Feuille de route

Chaque étape est livrable et vérifiable indépendamment.

| Étape | Contenu | État |
|---|---|---|
| **P0** | Squelette, intégration continue, cœur métier, tests de non-régression des dix défauts de 2017 | ✅ fait |
| **P1** | Filtre de Kalman, estimation d'assiette, inhibition du son, jeux de données synthétiques | ✅ fait |
| **P2** | Couche d'affichage 400×240, quatre écrans, tests par images de référence | ✅ fait |
| **P3** | Journal de vol, table de schéma des réglages, trames LK8EX1, boutons | ✅ fait |
| **P4** | Pilotes matériel : MS5611, centrale inertielle, écran Sharp, son, boutons. Tâches FreeRTOS | ✅ écrit et **compile** |
| **P5** | Harnais de rejeu : enregistrement des trames brutes en USB, rejeu en test | ✅ fait |
| **P6** | Portail Wi-Fi de configuration, BLE vers XCTrack | ✅ cœur **testé**, périphérie **écrite et compile** |
| **P7** | Circuit imprimé KiCad et sorties de fabrication | 🚧 schéma + PCB routé (auto) + Gerbers JLCPCB, **relecture humaine à faire** |
| **P8** | Boîtier paramétrique OpenSCAD | 🚧 brouillon rendu, à figer sur le PCB |
| **P9** | Documentation de montage illustrée | à venir |

## Ce qui reste faisable sans matériel

Le cœur logiciel est terminé. Le **harnais de rejeu** (P5) l'est aussi : format
de trame brute, émission USB, enregistrement, rejeu natif et tests (dont le
silence au repos).

Le **portail de configuration** (P6) est fait côté logiciel : la page web se
dessine depuis la table de schéma, et sa production (schéma + valeurs en JSON) et
son application (POST JSON borné) sont du cœur pur, couverts par des tests. Le
transport — point d'accès Wi-Fi, serveur HTTP, service BLE Nordic UART émettant
les trames LK8EX1, persistance NVS — est écrit et **compile** contre les talons,
mais ne peut être validé que sur le matériel (voir la recette).

Reste, sans matériel : cette **documentation**.

## Matériel : schéma et boîtier

Le **schéma KiCad** (P7) de la variante intégrée est engendré depuis une table
de connexions et validé électriquement — ERC propre, netlist exportable,
brochages vérifiés contre les bibliothèques KiCad et, pour l'élévateur, contre
son datasheet. Le **circuit imprimé** est ensuite engendré, placé et **routé
automatiquement** (freerouting, ~85 nets sur 86), et les **Gerbers + BOM + CPL
JLCPCB** sont exportés. Détail dans [Circuit imprimé](pcb.md). Reste une
**relecture humaine dans KiCad** avant commande : le routage auto passe le DRC
mais ne garantit ni le plan de masse, ni la zone d'antenne, ni la paire USB.

Le **boîtier** (P8) est un modèle OpenSCAD paramétrique dont les deux coquilles
se rendent en STL et en aperçu (`make case`). C'est un **brouillon** : ses cotes
dépendent du PCB, qui n'est pas encore tracé, et il n'a ni support de cartes ni
logement défini pour le piézo. Il sera repris une fois le circuit figé — voir
[Boîtier](boitier.md).

## Ce qui exige le matériel en main

Les pilotes (P4) peuvent être écrits mais pas validés. Tout ce qui touche au capteur, à l'écran, au son et à la batterie ne peut être que compilé — la validation demande un montage sur table.

## Le harnais de rejeu, à ne pas repousser

C'est le meilleur investissement de tout le projet, et il est facile à remettre à plus tard à tort.

Le principe : le firmware émet les trames brutes — horodatage, accéléromètre, gyromètre, pression, température — sur le port USB à environ 100 Hz, soit 4 ko/s, rien pour de l'USB natif. Un script les enregistre en CSV, et une suite de test rejoue un CSV **à travers toute la chaîne** en natif.

Conséquence : chaque comportement bizarre constaté en vol devient un enregistrement de 30 s, qui devient un fichier de référence versionné, qui devient un test de non-régression permanent. Et le réglage du filtre se fait sur dix vrais vols en une seconde sur un portable, au lieu de dix week-ends sur une pente.

Pour un appareil dont la justesse se définit par une physique qu'on ne peut pas reproduire sur un bureau, cela vaut plus que tous les autres tests réunis.

## Recette matérielle

Liste de contrôle à dérouler quand le montage existera.

1. **Silence au repos** — critère d'acceptation phare. Appareil posé sur une table, seuil à 0,2 m/s : vario inférieur à 0,05 m/s affiché et **aucun bip**. Le vario de 2017 échoue à ce test.
2. **Altitude** — comparer au QNH METAR d'un aérodrome proche et à une altitude connue ; écart attendu inférieur à 3 m.
3. **Latence** — montée en ascenseur ou en escaliers, chronométrer la réponse du son ; cible ~0,2 s contre ~0,8 s avant.
4. **Étanchéité à la lumière du baromètre** — braquer une lampe sur le boîtier fermé : aucune variation d'altitude.
5. **Insensibilité au souffle** — souffler sur le boîtier : aucun pic de vario.
6. **Froid** — trente minutes au congélateur : écran et capteur toujours opérationnels. C'est l'un des arguments du choix du Sharp contre l'e-paper.
7. **Virage** — spirale à inclinaison constante en tenant une altitude : la vitesse verticale ne doit pas dériver. C'est la validation en vrai du test synthétique de compensation centrifuge.
8. **Autonomie** — décharge complète en enregistrant la tension ; cible supérieure à 10 h avec le BLE actif.
9. **BLE** — appairage avec XCTrack, vérifier que l'altitude et le vario remontent.
10. **Audio en vol** — audible par-dessus le bruit du vent à 40 km/h.
11. **Vol de recette** — un vol complet, trace comparée à celle d'un vario de référence d'un copain de club.
