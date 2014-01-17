Arduino-variometer
==================

Projet de variomètre sur Arduino :
--------------

Le but de ce projet est de fabriquer un variomètre à faible coût pour une utilisation en vol libre (dans mon cas la pratique du parapente).
Voici à titre indicatif le matériel utilisé et leur prix d'achat sur eBay international :

- Arduino nano V3.0	5,10 €
- Nokia LCD 5110	2,03 €
- BMP085 Digital Barometric Pressure Sensor	2,88 €
- Digital Push Button Switch Rotary Encoder	4,21 €
- Mini USB Battery Charging Board 5V	0,93 €
- Buzzer	0,16 €
- DC-DC Converter Step Up 1-5V 500mA	1,13 €
- 3,7V 600mAh LiPo Battery	3,25 €
- RTC Module for Arduino	1,54 €
- 3x 10k Ohm resistor	0,03 €
- Right Angle Mini Slide Switch	0,10 €	
	
**TOTAL	21,36 €**


Présentation
==================

Ce variomètre utilise un minimum de commande pour naviguer dans le menu et interargir avec l'interface. Nous utilisons un encodeur digital permettant trois actions: Gauche - Droite - Valider
L'écran est celui d'un Nokia 5110, donc plutôt petit comparé aux variomètres du commerce. Mais les informations essentielles y sont affichées.

En plus de fournir des informations en temps réel, un système de détection automatique du début et de fin de vol permet d'obtenir des statistiques de vol intéressantent.

Menu :
--------------

- Vario
- Stats
	- Retour
	- Chrono
	- AltMin
	- AltMax
	- Tx Max
	- Tx Min
	- Reset
- Options
	- Retour
	- Tare
	- Alti
	- Montee
	- Desc
	- Light
	- Contra
	- Heure
	- Minute

Vario :
--------------

Affichage sur l'écran des éléments suivants :

- Altitude (m)
- Variation d'altitude (m/s) avec jauge dynamique
- Temps de vol (H:m:s)
- Heure
- Température

Le réglage du volume est possible en tournant l'encodeur digital.
Lors d'un appui long sur le poussoir de l'encodeur, les statistiques sont réinitialisés.


Stats :
--------------

L'enregistrement des statistiques de vol se déclenche automatiquement.
Pour cela le variomètre détecte une prise ou une baisse d'altitude significative entrainant le status "en vol".
Lorsqu'il n'y a pas de baisse ou de prise d'altitude significative depuis un certain temps, le status "en vol" s'arrête.

Voici les informations statistiques disponibles :

**Chrono**

Temps de vol enregistré, exprimé en heure : minute : seconde.

**AltMin**

L'altitude minimum en mètre enregistré pendant le vol.

**AltMax**

L'altitude maximum en mètre enregistré pendant le vol.

**Tx Max**

Le taux de chute maximum en mètre par seconde enregistré pendant le vol.

**Tx Min**

Le taux de chute minimum en mètre par seconde enregistré pendant le vol.

**Reset**

Remise à zéro des statistiques.


Options :
--------------

**Tare**

Initialise l'altitude zéro sur l'altitude actuelle.

**Alti**

Permet d'ajuster l'altitude.

**Montee**

Taux de montée déclenchant le bip.

**Desc**

Taux de descente déclenchant le bip.

**Light**

Instensité du rétro-éclairage (Off à 5)

**Contra**

Ajustement du contraste de l'écran.

**Heure**

Réglage de l'heure de la pendule.

**Minute**

Réglage des minute de la pendule.