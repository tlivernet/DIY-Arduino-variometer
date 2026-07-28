# Carte porteuse : la variante A

## Pourquoi le PCB intégré coûte 200 $ et pas 5 $

Ce n'est pas le circuit imprimé qui coûte cher. Un deux-couches de 100 × 100 mm chez JLCPCB vaut **2 à 5 $ les cinq**. Ce sont les frais d'**assemblage**, et ils sont presque entièrement **fixes** :

| Poste | Coût | Dépend de la quantité ? |
|---|---|---|
| Circuit imprimé nu, 5 exemplaires | 2 à 5 $ | un peu |
| Programmation des machines + pochoir laser | frais d'étude, une fois | **non** |
| Chargement d'un composant « étendu » dans un distributeur | **3 $ par référence unique** | **non** |
| Composants | selon la nomenclature | oui |

La nomenclature de la variante B compte 28 lignes, dont la plupart sont des composants « étendus ». À 3 $ la référence, cela fait **déjà 60 à 80 $ rien qu'en chargement de distributeurs**, avant d'avoir acheté un seul composant.

Autrement dit : **ces 200 $ sont à peu près les mêmes pour 5 cartes que pour 50**. À 50 exemplaires, la variante B revient à ~10 $ pièce et devient le bon choix. À l'unité, c'est absurde.

## La carte porteuse

La variante A a toujours été prévue comme la première livrée, et c'est exactement la réponse à ce problème : une carte qui ne fait **qu'interconnecter des modules déjà assemblés**.

Ce qu'il reste à souder dessus, en tout et pour tout :

- des barrettes femelles pour recevoir les modules ;
- **un** circuit intégré, en boîtier DIP soudable à la main : le tampon audio ;
- trois boutons, un interrupteur, quatre résistances, trois condensateurs.

| | Variante B intégrée | Carte porteuse |
|---|---|---|
| Circuit imprimé nu, 5 ex. | ~5 $ | ~5 $ |
| Assemblage | 150 à 200 $ | **0 $** |
| Composants à souder | 28 références CMS | 17 références traversantes |
| Outillage | four ou station à air chaud | un fer à souder |
| Modules à acheter en plus | aucun | ~20 € |
| Réparable | difficilement | on remplace un module |

Et un avantage qui compte pendant la mise au point : si le MS5611 reçu est une contrefaçon, **on change le module**, on ne refait pas la carte.

## Le tableau de câblage

`hardware/carte-porteuse/brochage.csv` décrit chaque liaison. Il est **vérifié automatiquement** contre `src/app/pins.h` :

```bash
make pinout
```

Un plan de câblage qui diverge du firmware est pire que pas de plan du tout : on soude d'après le document, le firmware pilote d'autres broches, et le symptôme ressemble à un composant défectueux. Le contrôle rend la divergence impossible, et il refuse aussi les broches inutilisables sur la carte visée — GPIO 33 à 37 non sortis sur l'ESP32-S3-Zero, GPIO 19 et 20 pris par l'USB natif, GPIO 43 et 44 par la console série, GPIO 21 par la LED RGB — ainsi qu'une mesure de batterie qui ne serait pas sur ADC1.

Il tourne en intégration continue.

## Points de conception

**Le rail 5 V est optionnel mais recommandé.** Il alimente l'écran Sharp, caractérisé à 5 V, et le tampon audio. Sans lui l'écran fonctionne à 3,3 V avec un contraste réduit, et le son perd environ 3,6 dB. Un module élévateur MT3608 à 0,50 € suffit — **régler sa sortie à 5,0 V avant de le câbler**, il sort souvent bien plus à la livraison.

**Le tampon audio est le seul circuit intégré à souder.** Un `SN74HCT125N` en DIP-14 se soude au fer sans difficulté. La famille **HCT** est impérative : ses seuils TTL font qu'une sortie 3,3 V de l'ESP32 le commande de façon fiable alors qu'il est alimenté en 5 V.

**Deux GPIO peuvent être libérés** si la place manque : `DISP_ON` peut être relié en permanence à 3V3, et `EXTCOMIN` à la masse si l'on utilise le basculement VCOM logiciel, que le pilote sait déjà faire. On passe alors de 14 à 12 broches.

**L'implantation compte plus que le routage.** Le MS5611 doit être en chambre borgne, loin du piezo, et sans aucune source lumineuse à proximité. Prévoir sa position sur la carte en fonction du boîtier, pas l'inverse.

## Sans circuit imprimé du tout

Le même tableau de câblage sert de plan pour une plaque à trous. C'est même le chemin le plus rapide pour la mise en route : on câble, on déroule la [liste de contrôle matérielle](mise-en-route.md), et on ne commande une carte qu'une fois le brochage confirmé sur le montage réel.

C'est l'ordre recommandé. Commander un circuit imprimé avant d'avoir vérifié l'ordre des bits de l'écran et l'orientation de la centrale inertielle, c'est risquer de payer deux fois.
