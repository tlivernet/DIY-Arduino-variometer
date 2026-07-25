# Variomètre ESP32-S3

Refonte complète du [variomètre DIY sur Arduino Nano](../README.md) de 2014-2017, pour du matériel sourçable en 2026 et une chaîne de développement qui fonctionne encore dans dix ans.

> **État : étape P0.** Seul le coeur métier existe, entièrement testé. Les pilotes matériel, le PCB et le boîtier arrivent aux étapes suivantes (voir [Feuille de route](#feuille-de-route)). **Rien de tout ceci n'a encore volé.**

## Pourquoi une refonte

Le montage d'origine fonctionne et il vole, mais :

- il ne compile que sur l'IDE Arduino 1.5-1.6.1, sorti en 2015 ;
- le BMP085 et le DS1307 ne se trouvent plus ;
- le vario a ~0,8 s de retard et ~0,3-0,4 m/s de bruit affiché, soit **plus que son propre seuil de déclenchement** à 0,3 m/s — d'où un bipeur qui bavarde à l'arrêt ;
- il n'a aucun test, et dix bugs identifiés dont deux graves (chrono qui ne s'arrête jamais, enregistrement qui cesse au bout de 8 vols).

## Ce qui est déjà là

Le coeur métier, en C++17 pur — aucune dépendance à Arduino, donc compilable et testable sur PC :

| Module | Rôle |
|---|---|
| `core/isa` | conversion pression → altitude (atmosphère standard), QNH borné |
| `core/vario_filter` | lissage pression + dérivée, coefficients indépendants de la cadence |
| `core/tone_law` | lois hauteur/durée des bips, profils « classique » et « moderne » |
| `core/tone_scheduler` | machine à états du bipeur : amorce continue, note, pause, hystérésis |
| `core/flight_fsm` | détection automatique décollage / atterrissage |
| `core/stats` | statistiques de vol (extrema, gain cumulé) |
| `core/battery` | jauge de batterie 1S |
| `core/format` | mise en forme de la vitesse verticale |

**70 tests** répartis en 7 suites, dont un test de non-régression nommé pour chacun des dix bugs de la version AVR.

## Compiler et tester

```bash
# Tests du coeur métier — c'est ce que vérifie la CI
make test

# Une seule suite
make test-test_flight_fsm

# Firmware embarqué (nécessite PlatformIO)
pio run -e esp32s3
```

`make test` n'a besoin que de `g++` et `make` : Unity est inclus dans le dépôt (`test/support/unity`, licence MIT). `pio test -e native` fait la même chose via PlatformIO si tu préfères.

## Matériel visé

| Poste | Choix | Prix indicatif |
|---|---|---|
| MCU | ESP32-S3 (USB natif, BLE 5, 2 cœurs) | 5 € |
| Baromètre | MS5611 (derrière une interface, un BMP390 doit pouvoir le remplacer) | 4 € |
| IMU | 6 axes, pour le vario compensé en accélération | 2 € |
| Écran | Sharp Memory LCD LS027B7DH01, 400×240 | 18 € |
| Audio | piezo large bande + tampon 5 V | 1 € |
| Commande | 3 boutons étanches | 1 € |
| Énergie | LiPo 1S 1500 mAh + charge USB-C | 6 € |
| Divers + PCB + PETG | | 5 € |
| **Total** | | **≈ 42 €** |

À comparer aux 26,85 € de la version 2014 (avec un écran à 2 €) et aux 250-400 € d'un vario du commerce.

Le Sharp Memory LCD a été retenu plutôt qu'un e-paper : même lisibilité en plein soleil et consommation quasi nulle à l'affichage figé, mais rafraîchissement rapide et **fonctionnement par grand froid** — un e-paper standard ralentit fortement sous 0 °C, ce qu'on rencontre couramment à 3000 m.

## Apports par rapport à la version AVR

- **Vario compensé en accélération** : fusion baro + accéléromètre par filtre de Kalman. La latence ne vient plus du filtre mais de l'intégration directe de l'accélération — d'où ~0,2 s au lieu de ~0,8 s, *et* un bruit divisé par 4 à 8 en même temps.
- **Seuil de montée utilisable à 0,2 m/s**, puisque le plancher de bruit descend sous 0,1 m/s.
- **Configuration au navigateur** par portail Wi-Fi, au lieu d'une molette et d'un écran de 84×48.
- **BLE vers XCTrack / XCSoar** (trames LK8EX1) : le vario sert aussi de capteur au téléphone.
- **Journal de vol append-only** en LittleFS, avec CRC et récupération après coupure — un vol interrompu par une batterie vide n'est plus perdu.
- **Silence au repos**, garanti par un test.

## Les dix bugs de 2017, et leur test

Chaque ligne correspond à un test de non-régression nommé dans `test/`.

| # | Bug | Test |
|---|---|---|
| 1 | Coefficients de filtre par itération, pas par Δt | `test_filter_is_sample_rate_independent` |
| 2 | `micros()` dans un `float`, comparé à travers son débordement à 71,6 min | `test_works_far_beyond_32bit_micros_wrap` |
| 3 | Aucune garde sur Δt → NaN définitif, écran et son morts | `test_zero_dt_is_rejected_without_nan` |
| 4 | Ligne de base en `int` → **le chrono ne s'arrête jamais** (~1 atterrissage sur 4) | `test_landing_detected_at_fractional_altitude` |
| 5 | État mort après 8 vols → **plus aucun enregistrement** | `test_many_consecutive_flights_are_all_detected` |
| 6 | `map()` sous-débordant → batterie à plat affichée **pleine** | `test_deeply_discharged_battery_reads_empty` |
| 7 | QNH non borné → `pow(x/0,…)` → altitude NaN définitive | `test_qnh_is_clamped` |
| 8 | Report de retenue : 1,96 m/s affiché « 1.10 » | `test_rounding_carry_does_not_produce_ten` |
| 9 | Gain cumulé intégrant le bruit, puis débordement | `test_noise_does_not_inflate_cumulative_gain` |
| 10 | Offsets EEPROM par `sizeof` de struct (diffèrent AVR/ESP32) | remplacé par NVS clé-valeur (P8) |

Le bipeur qui bavarde à l'arrêt, principal défaut d'usage de l'ancien vario, est verrouillé par `test_hysteresis_prevents_chatter_at_threshold`.

## Feuille de route

| Étape | Contenu | État |
|---|---|---|
| P0 | Squelette, CI, coeur métier + tests de non-régression | **fait** |
| P1 | Pilotes MS5611, écran Sharp, audio LEDC, boutons ; tâches FreeRTOS | à venir |
| P2 | IMU + filtre de Kalman, inhibition du son pendant la convergence | à venir |
| P3 | Portail Wi-Fi de configuration + BLE LK8EX1 | à venir |
| P4 | PCB KiCad + gerbers JLCPCB | à venir |
| P5 | Boîtier paramétrique OpenSCAD | à venir |
| P6 | Site de documentation, BOM, guide de montage | à venir |

## Antériorité à consulter

- **[GNUVario-E](https://prunkdump.github.io/GNUVario-TTGO-T5-website/)** — projet français open source/open hardware très proche : ESP32, e-paper, MS5611 + MPU9250, boîtier 3D, docs FR/EN. À lire avant d'écrire du code.
- **[har-in-air/ESP8266_MPU9250_MS5611_VARIO](https://github.com/har-in-air/ESP8266_MPU9250_MS5611_VARIO)** — fusion Kalman « zéro lag », portail Wi-Fi. Source du choix MS5611 plutôt que BMP280 (pics de bruit) et du piezo large bande.
- [ESP32-Realtime-Vario](https://github.com/ghf20/ESP32-Realtime-Vario), [sensorio](https://github.com/zmolnar/sensorio), [VhARIO-ESPC3](https://github.com/har-in-air/VhARIO-ESPC3).

## Licence

À définir. Le code de 2017 n'en portait pas ; il faudra en choisir une (GPL-3.0 ou MIT) avant publication.
