DIY Arduino variometer - with BMP085 Barometric Pressure Sensor
==================

Il s'agit de la version utilisant le capteur de pression BMP085. 
Vous trouverez une version alternative de ce code supportant le MS5611 sur la branche "[MS5611](https://github.com/sinseman/DIY-Arduino-variometer/tree/MS5611)".

- [Mise à jour](#mise-à-jour)
- [Présentation](#présentation)
- [Schéma](#schéma)
- [Installation](#installation)
- [Interface et utilisation](#interface-et-utilisation)

## MISE À JOUR

**Attention !** Lors de la dernière mise à jour le schéma de connection de l'écran a été modifié pour des soucis d'optimisations du montage.

- **PIN\_SCLK** : D8 --> D4
- **PIN\_LIGHT** : D11 --> Inchangée
- **PIN\_SCE** : D7 --> Inchangée
- **PIN\_RESET** : D6 --> D8
- **PIN\_DC** : D5 --> D6
- **PIN\_SDIN** :  D4 --> D5

Bien sûr vous pouvez adapter le code suivant l'ordre des branchements de l'écran à votre disposition.

Cela est valable aussi avec l'encodeur digital pour inverser le sens de rotation en remplaçant ```
Encoder knob(3, 2);``` par ``` Encoder knob(2, 3);```

**Il faut également ré-initialiser la mémoire EEPROM en téléversant une première fois avec la variable initialisation à true puis re-téléverser avec initialisation à false (voir partie [Installation](#installation)).**

## Présentation
### Vidéo de démo

[![Youtube - DIY - Variomètre à base d'Arduino nano](http://img.youtube.com/vi/KeNAhEgbHnc/0.jpg)](http://www.youtube.com/watch?v=KeNAhEgbHnc)

[Participer à la discussion sur le forum parapentiste.](http://www.parapentiste.info/forum/bons-plans/variometre-maison-a-base-darduino-t33538.0.html)


### Matériel et coût

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
- 3x 10k Ohm resistor + 1x 120 Ohm resistor | 0,10€
- Right Angle Mini Slide Switch | 0,10€	
- Cables | 3€
- Plastic Electronics Project Box Enclosure DIY 27x60x100mm | 2,40€

**TOTAL	| 26,85 €**


## Schéma

![Schéma](https://github.com/sinseman/DIY-Arduino-variometer/blob/master/Variometer.png?raw=true)


## Installation

Une fois votre montage terminé il faut compiler et téléverser le code sur votre Arduino. Pour cela il vous faudra utiliser l'IDE Arduino.

**Il faut utiliser [une version de l'IDE Arduino >= 1.5 et <= 1.6.1](https://www.arduino.cc/en/Main/OldSoftwareReleases), sinon des erreurs de compilation apparaissent !**

Tout d'abord vous devez installer les librairies fournies avec le projet. Copiez le contenu du dossier librairies dans le dossier librairies de votre installation (par exemple C:\Users\Toto\Documents\Arduino\libraries)

Ensuite éditez avec l'IDE Arduino le fichier Variometer/Variometer.ino et cliquez sur le bouton "Vérifier" en haut du logiciel. Après un certain temps le message "Compilation terminée" devrait apparaitre. Si vous avez des erreurs vérifiez que les librairies sont bien installées (cf plus haut).

Une fois la compilation OK il faut téléverser le programme sur l'Arduino à l'aide d'un câble USB par exemple et en cliquant sur le bouton "Téléverser" de l'IDE. **Mais avant cela lisez ce qui suit !**

**Lors du premier téléversement du programme sur l'Arduino il est important de mettre dans le code la variable  initialisation à true.
Celle-ci permet d'initisaliser correctement la mémoire Eeprom (stockant les statistiques et les options).**

```
bool initialisation = true; 
```

**Une fois le premier allumage fait il faut donc re-téléverser le code avec cette fois-ci initialisation à false. Si cela n’est pas fait la mémoire Eeprom sera effacée à chaque allumage !**

```
bool initialisation = false; 
```


## Interface et utilisation


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
	- QNH
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
Pour cela le variomètre détecte une prise ou une baisse d'altitude significative entrainant le statut "en vol".
Lorsqu'il n'y a pas de baisse ou de prise d'altitude significative pendant un certain temps, le statut "en vol" s'arrête.

A la fin du vol les statistiques sont sauvegardées et ne sont pas perdues à l'extinction du variomètre.

Ce programme peut enregistrer 8 rapports de vol. Une fois un vol terminé, la piste suivante d'enregistrement est sélectionnée. Si celle-ci n'est pas vide elle n'est pas écrasée. Il faut alors manuellement effacer la plage de stat en cours (avec un appui long sur le bouton depuis l'interface Vario) ou reset toutes les plages (Menu Stats --> Reset).


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

**QNH**

Pression atmosphérique convertie au niveau de la mer selon les conditions de l'atmosphère standard.

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

