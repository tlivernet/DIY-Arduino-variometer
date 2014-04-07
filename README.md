DIY Arduino variometer
==================

- [Vidéo](#vidéo)
- [Matériel et coût](#matériel-et-coût)
- [Schéma](#schéma)
- [Installation](#installation)
- [Interface](#interface)

## Vidéo

![![Youtube - DIY - Variomètre à base d'Arduino nano](http://img.youtube.com/vi/KeNAhEgbHnc/0.jpg)](http://www.youtube.com/watch?v=KeNAhEgbHnc)


## Matériel et coût

Le but de ce projet est de fabriquer un variomètre à faible coût pour une utilisation en vol libre (dans mon cas la pratique du parapente).
Voici à titre indicatif le matériel utilisé et leur prix d'achat sur eBay international (prix de janvier 2014) :

- Arduino nano V3.0	| 5,10€
- Nokia LCD 5110 | 2€
- BMP085 Digital Barometric Pressure Sensor	| 2,90€
- Digital Push Button Switch Rotary Encoder	| 4,20€
- Mini USB Battery Charging Board 5V | 0,90€
- Buzzer 8 ohm | 0,20€
- DC-DC Converter Step Up 1-5V 500mA | 1,15€
- 3,7V 600mAh LiPo Battery | 3,25€
- RTC Module for Arduino | 1,55€
- 3x 10k Ohm resistor + 1x 100k Ohm resistor + 1x 120 Ohm resistor | 0,10€
- Right Angle Mini Slide Switch | 0,10€	
- Cables | 3€
- Plastic Electronics Project Box Enclosure DIY 27x60x100mm | 2,40€

**TOTAL	| 26,85 €**


## Schéma

![Schéma](https://github.com/sinseman/DIY-Arduino-variometer/blob/master/Variometer.png?raw=true)


## Installation

Lors du premier téléversement du programme sur l'Arduino il est important de passer la variable *initialisation* à *true*.
Celle-ci permet d'initisaliser correctement la mémoire Eeprom (stockant les statistiques et les options).

```c++
bool initialisation = true; 
```

Une fois le premier allumage fait il faut donc re-téléverser le code avec cette fois-ci *initialisation* à *false*. Si cela n’est pas fait la mémoire Eeprom sera effacée à chaque allumage.

```c++
bool initialisation = false; 
```

## Interface

Ce variomètre utilise un minimum de commande pour naviguer dans le menu et interagir avec l'interface. Nous utilisons un encodeur digital permettant trois actions: Gauche - Droite - Valider.

L'écran est celui d'un Nokia 5110, donc plutôt petit comparé aux variomètres du commerce, mais les informations essentielles y sont affichées. Ce variomètre produit des "bips" qui sont aussi bien émis lors de la navigation dans le menu que lors de l'utilisation de l'interface vario. 
Le volume est bien entendu paramétrable ainsi que la sensibilité de déclenchement en monté et en descente.

En plus de fournir des informations en temps réel, un système de détection automatique du début et de fin de vol permet d'obtenir des statistiques intéressantes.


### Menu

- Vario
- Stats
	- Retour
	- Statistiques
	- Reset
- Options
	- Retour
	- Tare
	- Alti
	- Montee
	- Desc
	- Light
	- Contra
	- Date

### Vario

Affichage dynamique des éléments suivants :

- Altitude (m)
- Variation d'altitude (m/s) avec jauge dynamique
- Temps de vol (heure : minute : seconde)
- Heure
- Température
- Batterie

Le réglage du volume est possible en tournant l'encodeur digital.
Lors d'un appui long sur le poussoir de l'encodeur, les statistiques sont réinitialisées.


### Stats

L'enregistrement des statistiques de vol se déclenche automatiquement.
Pour cela le variomètre détecte une prise ou une baisse d'altitude significative entrainant le statu "en vol".
Lorsqu'il n'y a pas de baisse ou de prise d'altitude significative depuis un certain temps, le statu "en vol" s'arrête.

A la fin du vol les statistiques sont enregistrés en dur donc ne sont pas perdus à l'extinction du variomètre.

Ce programme peut enregistrer 5 rapports de vol. Une fois un vol terminé, la piste suivante d'enregistrement est sélectionnée.


**Statistique**

Voici les informations statistiques disponibles :

- Date
	- Date et heure de début de vol.

- Chrono
	- Temps de vol enregistré, exprimé en heure : minute : seconde.

- AltMin
	- L'altitude minimum en mètre enregistré pendant le vol.

- AltMax
	- L'altitude maximum en mètre enregistré pendant le vol.

- Tx de chutte
	- Les taux de chute maximum et minimum en mètre par seconde enregistrés pendant le vol.

- Cumul
	- Altitude cumulée du vol (en mètre).

**Reset**

Remise à zéro des statistiques.


### Options

**Tare**

Initialise l'altitude zéro sur l'altitude actuelle.

**Alti**

Permet d'ajuster l'altitude.

**Montee**

Taux de montée déclenchant le bip.

**Desc**

Taux de descente déclenchant le bip.

**Light**

Intensité du rétro-éclairage (Off à 5)

**Contra**

Ajustement du contraste de l'écran.

**Date**

Réglage de la date et de l'heure de la pendule.

