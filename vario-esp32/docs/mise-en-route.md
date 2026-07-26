# Mise en route du matériel

!!! danger "Aucune de ces étapes n'a été exécutée"
    Les pilotes ont été écrits et leur syntaxe vérifiée, mais **rien n'a jamais tourné sur du matériel**. Cette page est une liste de contrôle à dérouler dans l'ordre, pas un compte rendu. Chaque étape isole une cause de panne, ce qui évite de chercher trois problèmes à la fois.

## Ce qui a été vérifié, et ce qui ne l'a pas été

| | État |
|---|---|
| Cœur métier (`src/core`, `src/ui`) | **166 tests exécutés**, tous verts |
| Rendu des écrans | **exécuté**, images comparées à des références |
| Syntaxe de `src/hal` et `src/app` | **vérifiée** contre des talons Arduino/ESP-IDF |
| Compilation pour l'ESP32-S3 | **vérifiée** — RAM 9,9 %, flash 12,2 % |
| Fonctionnement des pilotes | **non vérifié** |

Le firmware compile désormais pour la cible, sur Arduino-ESP32 2.0.17 et PlatformIO espressif32 6.12 :

```
RAM :    9,9 %  (32 340 / 327 680 octets)
Flash : 12,2 % (320 429 / 2 621 440 octets)
```

Il reste donc largement de quoi ajouter le Wi-Fi et le BLE, qui coûteront ~1 Mo de flash et ~100 Ko de RAM.

Deux erreurs ont été nécessaires pour y arriver, toutes deux invisibles au contrôle sur talons puisqu'elles relevaient de la **configuration** et non de la syntaxe : l'absence de `build_unflags = -std=gnu++11`, qui faisait compiler en C++11, et deux en-têtes manquants. Un `static_assert` dans `core/clock.h` rattrape désormais le premier cas avec un message explicite.

## 1. Le calculateur seul

Avant tout câblage : flasher, ouvrir le port série à 115200. La ligne `Variometre ESP32-S3` doit apparaître.

## 2. Le bus I2C

Câbler le baromètre et la centrale inertielle, puis lancer un scanner I2C. Deux adresses doivent répondre : **0x77** pour le MS5611 (module GY-63) et **0x68** pour la centrale.

Si rien ne répond : vérifier l'alimentation, les tirages au plus (souvent présents sur les modules), et que SDA et SCL ne sont pas inversés.

## 3. Le baromètre

Au démarrage, `Ms5611::begin()` refuse un étalonnage dont tous les coefficients valent 0 ou 0xFFFF — cas typique d'un module contrefait ou d'un bus mal câblé.

- La pression affichée doit être plausible : **950 à 1030 hPa** au niveau de la mer, moins en altitude.
- La température doit être crédible à quelques degrés près.
- Compter les lignes de diagnostic : `errorCount()` doit rester à zéro.

**Test décisif** : monter puis descendre l'appareil d'un mètre à la main. L'altitude doit suivre de ~1 m. Le MS5611 a une résolution de 10 cm, donc ce mouvement est parfaitement visible.

## 4. L'orientation de la centrale inertielle

!!! warning "L'erreur la plus coûteuse"
    L'orientation du module sur la carte est un choix mécanique. Une permutation ou un signe faux donne une assiette fausse, donc un vario faux — et cela ne se voit pas au sol.

Appareil posé à plat, le repère visé est **X vers l'avant, Y vers la droite, Z vers le bas**. À vérifier :

| Position | Attendu |
|---|---|
| à plat, immobile | accélération verticale calculée ≈ 0 |
| incliné vers la droite | composante Y de l'accéléromètre change de signe |
| pivoté à plat vers la droite | composante Z du gyromètre positive |

Corriger au besoin `Mpu6050::AxisMap` dans la configuration, sans toucher au câblage.

## 5. L'écran

!!! warning "L'ordre des bits est le point le plus susceptible d'être faux"
    Le Sharp reçoit ses octets en commençant par le bit de poids faible, et ce premier bit correspond au pixel le plus à gauche. Notre tampon stocke le pixel de gauche dans le bit de poids fort, ce qui donne le bon ordre en transmettant MSB d'abord. Mais l'octet de commande et l'adresse de ligne sont des **nombres**, pas des motifs de pixels, et doivent être inversés avant émission.

Symptômes et remèdes :

| Symptôme | Réglage à changer |
|---|---|
| image miroitée par groupes de 8 pixels | `reverse_pixel_bytes = true` |
| image brouillée ou lignes dans le désordre | `reverse_control_bytes` |
| écran noir | vérifier DISP, et que **CS est actif à l'état HAUT** |
| écran qui pâlit avec le temps | le basculement VCOM ne se fait pas — vérifier `maintain()` |

Rappels : horloge à **1 MHz**, CS **actif haut**, et vérifier la tension d'alimentation du panneau — sa caractérisation est faite à 5 V, il fonctionne à 3,3 V avec un contraste réduit.

## 6. Le son

- Sans le tampon 5 V, le son sera **audible mais faible**, environ 3,6 dB sous le montage AVR.
- Vérifier le déphasage à l'oscilloscope : les deux broches doivent être en opposition exacte.
- Balayer la plage de fréquences et repérer la résonance du piezo : c'est là qu'il rend le mieux, et le profil sonore peut être recentré dessus.

Mesurer le niveau sonore au smartphone, puis ajuster les rapports cycliques des quatre niveaux de volume : la réponse d'un piezo au rapport cyclique n'est pas linéaire.

## 7. Les boutons

Appui bref, appui long, répétition automatique sur les flèches, et absence de répétition sur « valider ». Vérifier aussi qu'un bouton **déjà enfoncé au démarrage** ne produit aucun événement au relâchement.

## 8. La batterie

Comparer la lecture à un voltmètre. L'écart doit être inférieur à 50 mV après calibration du rapport du diviseur. Vérifier que la broche utilisée est bien sur **ADC1** : l'ADC2 devient inutilisable dès que le Wi-Fi est actif.

## 9. Recette complète

Une fois tout ce qui précède validé, dérouler la [recette matérielle](feuille-de-route.md#recette-materielle) : silence au repos, altitude comparée au QNH METAR, latence, étanchéité à la lumière du baromètre, insensibilité au souffle, froid, virage, autonomie, BLE, audio en vol, puis vol de recette.

Le critère phare reste le premier : **appareil posé sur une table, seuil à 0,2 m/s, aucun bip**. C'est le test que le vario de 2017 échoue.
