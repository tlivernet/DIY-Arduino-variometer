# Où acheter, et avec quel fichier

## Réponse courte par plateforme

| Plateforme | Téléversement d'une nomenclature ? | Fichier à utiliser |
|---|---|---|
| **Octopart, TrustedParts, FindChips** | oui | `bom-composants-upload.csv` |
| **LCSC** | oui, jusqu'à 800 lignes et 4 Mo | `bom-composants-lcsc.csv` |
| **JLCPCB** (assemblage) | oui | `bom-composants-jlcpcb.csv` |
| **AliExpress** | **non** | aucun — voir la liste ci-dessous |
| **Alibaba.com** | indirectement, par demande de devis | n'importe lequel, en pièce jointe |

```bash
make bom   # engendre tous ces fichiers depuis la nomenclature de référence
```

Les fichiers dérivés sont **engendrés**, jamais maintenus à la main : chaque distributeur veut ses colonnes, et entretenir quatre copies d'une même nomenclature garantit qu'elles finissent par diverger.

## AliExpress : il n'y a rien à téléverser

AliExpress est une place de marché grand public. Il n'existe ni outil de nomenclature, ni panier construit depuis un fichier, ni interface de programmation ouverte aux particuliers. Chaque article se cherche et s'ajoute au panier à la main.

Ce qu'on trouve en cherchant « BOM service AliExpress », ce sont des vendeurs de **Alibaba.com** — le site B2B frère — qui proposent un « BOM list service » comme produit : on leur envoie le fichier par message, ils répondent par un devis. C'est pertinent à partir de plusieurs dizaines de cartes, pas pour un exemplaire.

!!! tip "L'alternative qui répond vraiment au besoin"
    **LCSC** est dans la même gamme de prix qu'AliExpress, expédie depuis la Chine, et accepte le téléversement d'une nomenclature. Il vend des composants et non des modules, donc c'est la variante B qui s'y commande. Et comme c'est la même maison que JLCPCB, on peut enchaîner directement sur la fabrication et l'assemblage de la carte.

## Liste de courses AliExpress — variante A

À chercher un par un. Les contrôles indiqués comptent autant que le prix : sur ces plateformes, une partie des modules n'est pas ce qu'elle prétend être.

### Carte de calcul

Rechercher **« ESP32-S3-Zero Waveshare »**. Préférer la boutique officielle Waveshare. Vérifier que c'est bien un **S3** et non un C3, qui n'a qu'un cœur et pas d'USB natif.

### Baromètre — le poste le plus risqué

Rechercher **« GY-63 MS5611 »**.

!!! danger "Les modules MS5611 sont massivement réétiquetés"
    C'est le composant le plus contrefait de cette liste. On reçoit parfois un BMP280, parfois une puce inconnue sous une sérigraphie MS5611.

    Le firmware refuse déjà au démarrage un étalonnage dont les six coefficients valent tous 0 ou tous 0xFFFF, cas typique d'une contrefaçon grossière ou d'un bus mal câblé. Mais un faux plus soigné passera ce contrôle.

    Le test qui tranche : poser l'appareil immobile et regarder le bruit sur l'altitude. Un vrai MS5611 en suréchantillonnage 4096 donne **environ 10 cm**. Un faux donne facilement dix fois plus, et cela se voit immédiatement sur la courbe des 30 secondes.

Prendre deux exemplaires chez deux vendeurs différents : c'est 4 € d'assurance sur le composant qui détermine toute la qualité de l'instrument.

### Centrale inertielle

Rechercher **« GY-521 MPU6050 »**. Beaucoup de modules vendus comme MPU6050 contiennent en réalité un MPU6500, qui convient tout aussi bien ici. Le registre d'identification les distingue : 0x68 pour un MPU6050, 0x70 ou 0x71 pour un MPU6500 ou 9250. Le pilote accepte les deux et affiche lequel il a trouvé.

### Écran

Rechercher **« LS027B7DH01 »** ou **« Sharp Memory LCD 2.7 »**. Vérifier la référence exacte : les variantes 1,3" et 2,7" se ressemblent sur les photos. Sans carte d'adaptation, la nappe est en FPC 0,5 mm à 10 voies, **impossible à souder sur plaque à trous** — prendre la version avec carte de découpe, ou la carte Adafruit 4694.

### Batterie

Rechercher **« LiPo 1S 1500mAh protected »**, avec circuit de protection intégré.

!!! warning "Les capacités annoncées sont très souvent fausses"
    Contrôle simple et fiable : **la masse**. Une cellule LiPo 1S de 1500 mAh stocke 5,5 Wh, ce qui à la densité d'énergie réelle de ces cellules représente **30 à 40 g**. Une cellule annoncée à 1500 mAh qui pèse 15 g fait en réalité moins de la moitié de sa capacité annoncée. Une balance de cuisine suffit.

### Charge

Rechercher **« TP4056 module protection »**. Vérifier la présence des **deux puces supplémentaires** à côté du TP4056 : un DW01A et un double MOSFET FS8205. Sans elles, le module charge mais ne protège pas contre la décharge profonde ni le court-circuit. Les deux versions coexistent au même prix.

### Horloge

Rechercher **« DS3231 module »**. Attention à un piège fréquent : beaucoup de modules sont livrés avec une pile **non rechargeable** LIR2032 ou CR2032 câblée sur un circuit de charge. Retirer la résistance de charge, ou monter une pile réellement rechargeable.

### Son et commande

Rechercher **« piezo disc 20mm »** — prendre un disque nu, pas un buzzer encapsulé à fréquence fixe, puisque la hauteur du son doit balayer une plage. Et **« tactile switch 6x6 THT »** avec capuchons silicone.

### Ne pas oublier le tampon audio

C'est le seul composant de cette liste qui ne se trouve pas bien sur AliExpress : une porte logique **74HCT2G34** ou **SN74HCT125**, alimentée en 5 V. Elle vaut 0,15 € chez LCSC ou n'importe quel distributeur, et sans elle le son perd environ 3,6 dB — sur le critère le plus important d'un variomètre.

## À l'arrivée du colis

Avant d'assembler quoi que ce soit, dérouler la [mise en route du matériel](mise-en-route.md). Elle isole une cause de panne par étape, ce qui évite de chercher un module contrefait, une erreur d'orientation de capteur et un ordre de bits inversé en même temps.
