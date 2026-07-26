# Pourquoi une refonte

Le variomètre de 2014-2017 fonctionne et il vole. Ce n'est pas la raison de cette refonte.

## Le projet n'est plus constructible

Le code est un unique fichier de 1131 lignes qui **ne compile que sur l'IDE Arduino 1.5 à 1.6.1**, sorti en 2015. Reconstruire l'appareil aujourd'hui demande d'abord de faire de l'archéologie logicielle. Et les composants ne se trouvent plus : BMP085 en fin de vie, DS1307 sans compensation thermique, écran Nokia 5110 de 84×48 pixels, ATmega328P avec 2 ko de RAM et aucune radio.

## Le vario était plus bruyant que son propre seuil

C'est le point technique décisif. Le filtrage était constitué de deux moyennes glissantes en cascade :

```cpp
// pression
average_pressure = average_pressure * 0.95 + event.pressure * 0.05;
// vitesse verticale
vario = vario * 0.8 + (200000 * ((Altitude - alt) / (tempo - tim)));
```

À la cadence de boucle d'environ 30 Hz, cela donnait des constantes de temps de ~645 ms et ~148 ms, soit **~0,8 s de retard de groupe**. Et comme la dérivée amplifie le bruit du capteur, la vitesse verticale affichée portait **0,3 à 0,4 m/s de bruit RMS** — c'est-à-dire *davantage* que le seuil de déclenchement par défaut, fixé à 0,3 m/s.

Conséquence concrète : l'appareil bipait tout seul, posé sur une table. Et comme la machine à états du bipeur n'avait aucune hystérésis, chaque franchissement de seuil relançait l'amorce sonore continue de 1,35 s, produisant des chaînes de sons dans l'air calme.

## Dix défauts, et un test pour chacun

L'analyse ligne par ligne de l'ancien code en a fait ressortir dix. Chacun a maintenant un test de non-régression nommé dans le nouveau dépôt.

| # | Défaut | Test |
|---|---|---|
| 1 | Coefficients de filtrage fixés par itération de boucle, pas par Δt : la réponse dépendait de la cadence d'appel | `test_filter_is_sample_rate_independent` |
| 2 | `micros()` stocké dans un `float` (quantum de 512 µs après une heure) et comparé à travers son débordement à 71,6 min | `test_works_far_beyond_32bit_micros_wrap` |
| 3 | Aucune garde sur Δt : un NaN définitif tuait silencieusement écran **et** son | `test_zero_dt_is_rejected_without_nan` |
| 4 | Ligne de base de détection en `int` : **le chrono ne s'arrêtait jamais** | `test_landing_detected_at_fractional_altitude` |
| 5 | État mort après huit vols : **plus aucun enregistrement** | `test_more_than_eight_flights_keeps_recording` |
| 6 | `map()` sous-débordant : une batterie à plat s'affichait **pleine** | `test_deeply_discharged_battery_reads_empty` |
| 7 | QNH non borné : `pow(p/0, …)` puis altitude NaN définitive | `test_qnh_is_clamped` |
| 8 | Report de retenue : 1,96 m/s s'affichait « 1.10 » | `test_rounding_carry_does_not_produce_ten` |
| 9 | Gain cumulé intégrant le bruit à 30 Hz, puis débordement | `test_noise_does_not_inflate_cumulative_gain` |
| 10 | Offsets EEPROM calculés par `sizeof()`, différents entre AVR et ESP32 | remplacé par un stockage clé-valeur |

### Le n°4 : le chrono qui ne s'arrête jamais

Le plus insidieux. La détection d'atterrissage comparait l'altitude à une ligne de base :

```cpp
int altitude_temp;              // ← un int
altitude_temp = Altitude;       // ← troncature de l'altitude flottante
// ...
if (altitude_temp - 0.75 < Altitude && altitude_temp + 0.75 > Altitude) { ... }
```

En atterrissant à 1234,9 m, `altitude_temp` valait 1234 et la bande d'immobilité devenait (1233,25 ; 1234,75) — l'altitude réelle en était **déjà exclue**. La condition d'arrêt ne pouvait jamais devenir vraie, et le chronomètre de vol tournait indéfiniment. Cela touchait environ un atterrissage sur quatre, selon la partie fractionnaire de l'altitude d'arrivée.

### Le n°5 : l'enregistrement qui cesse

Après un atterrissage, `chrono_start` et `chrono` étaient tous deux non nuls. Or les deux blocs de détection testaient respectivement `chrono_start == 0` et `chrono == 0` : plus aucun ne s'exécutait. Le retour à l'état exploitable dépendait entièrement du chargement de l'emplacement suivant — et si celui-ci n'était pas vide, l'appareil **n'enregistrait plus jamais aucun vol**, silencieusement, au bout de huit vols.

Dans la refonte, ce défaut n'est pas corrigé mais rendu **impossible à écrire** : le journal est en ajout seul, il n'y a plus d'index d'emplacement à faire tourner, et le cycle de vie du vol ne connaît rien du stockage. Une panne d'écriture ne peut donc pas bloquer la machine à états.

## Ce qui est conservé

Le « ressenti » d'un vario est ce qui décide un pilote à l'adopter. Ces éléments sont repris à l'identique :

- la formule de l'atmosphère standard `44330 · (1 − (p/p₀)^0,1903)`. **Ignorer la température n'était pas un défaut** : un altimètre est par définition référencé à l'atmosphère standard, et « corriger » avec la température réelle désynchroniserait l'altitude affichée de celle de tous les autres pilotes et de XCTrack ;
- les lois sonores d'origine, disponibles sous le profil « classique » : hauteur `690 + 150·v` Hz, note `150 − 30·v` ms, pause égale à la moitié de la note ;
- **l'amorce sonore continue de 1,35 s** au premier franchissement de seuil, dont la hauteur suit le vario en direct. C'était un effet de bord d'un compteur d'itérations, mais c'est une vraie bonne idée : à l'entrée dans une ascendance, on entend le son monter avant que les bips ne s'installent ;
- les seuils par défaut, la convention « 0 désactive », le calage par Tare et décalage d'altitude, les huit emplacements « M1 » à « M8 » et les libellés de menu en français.
