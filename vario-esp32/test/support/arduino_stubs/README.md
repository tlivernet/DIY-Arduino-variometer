# Talons Arduino / ESP-IDF

Déclarations minimales permettant de **compiler** `src/hal` et `src/app` avec g++
sur un PC, pour attraper les fautes de frappe, les erreurs de types et les
incohérences internes.

## Ce que cela vérifie

- syntaxe et cohérence de tout le code écrit ici ;
- noms de membres, signatures et types de nos propres classes ;
- que la couche matérielle utilise correctement le cœur métier.

## Ce que cela NE vérifie PAS

- que les signatures réelles d'Arduino-ESP32 et de l'ESP-IDF correspondent à ces
  talons — elles changent entre versions majeures ;
- les valeurs de registres, les temporisations, l'ordre des bits sur le bus ;
- que quoi que ce soit fonctionne sur du vrai matériel.

Un talon qui diverge de l'API réelle donne un faux sentiment de sécurité. Ces
fichiers ne remplacent pas `pio run -e esp32s3`, ils le précèdent.

## Talons présents

Outre les périphériques de base (GPIO, I2C, SPI, LEDC, minuterie, FreeRTOS), les
briques réseau de P6 : `Preferences.h` (NVS), `WiFi.h` et `WebServer.h` (portail
de configuration), et la pile BLE (`BLEDevice.h` et ses en-têtes satellites
`BLEServer.h`, `BLEUtils.h`, `BLE2902.h`). Ces API sont fournies par le cœur
Arduino-ESP32 sur la cible ; aucune dépendance externe à déclarer dans
`platformio.ini`.
