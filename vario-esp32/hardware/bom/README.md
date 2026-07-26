# Nomenclatures

Deux nomenclatures, pour deux usages différents.

| Fichier | Contenu | À téléverser sur |
|---|---|---|
| `bom-composants.csv` | composants unitaires de la variante B (PCB intégré) | **Octopart, TrustedParts, FindChips** |
| `bom-modules.csv` | modules du commerce de la variante A | rien — voir ci-dessous |

## Pourquoi deux fichiers

Octopart, TrustedParts et FindChips indexent le catalogue de **distributeurs agréés** : Digi-Key, Mouser, Farnell, LCSC, Arrow. Ils cherchent des *composants*, pas des cartes de développement chinoises.

La nomenclature de la variante A ne s'y résoudra donc quasiment pas : un « GY-63 » ou un « ESP32-S3-Zero » n'a pas de référence fabricant au sens d'un distributeur. La colonne `Resoluble sur Octopart` de ce fichier le dit ligne par ligne. Elle reste utile comme liste de courses AliExpress / LCSC / Adafruit, pas comme fichier de téléversement.

## Utilisation

```bash
# Valider avant tout envoi
python3 ../../tools/bom_check.py

# Produire les fichiers réduits aux trois colonnes attendues
python3 ../../tools/bom_check.py --emit-upload
```

Les fichiers `*-upload.csv` ne contiennent que `MPN, Manufacturer, Quantity`. Les trois outils se contentent de cela, et fournir le strict nécessaire évite qu'une colonne mal interprétée fasse échouer l'appariement. Les fichiers riches restent la source de vérité, avec les repères, les boîtiers et la fonction de chaque pièce.

Le validateur refuse une référence vide, un fabricant vide, une quantité non entière, un repère dupliqué, et signale les caractères non ASCII qui passent mal dans certains téléversements.

## Ce que je n'ai PAS vérifié

!!! danger "À lire avant de commander"
    Les références ont été choisies pour être des pièces réelles et couramment distribuées, **mais rien n'a été vérifié en direct** : ni le stock, ni le prix, ni le délai, ni même que chaque suffixe de référence soit exactement le bon. C'est précisément le travail que font Octopart et TrustedParts — téléverse le fichier et lis ce qu'ils répondent.

Trois lignes sont marquées `A VERIFIER` dans la colonne de confiance, parce que le suffixe exact m'est incertain :

- **U8, `74HCT2G34GV125`** — il faut un tampon à seuils **TTL** (famille HCT) alimenté en 5 V, pour qu'une sortie 3,3 V de l'ESP32 le commande de façon fiable. Un `74LVC` en 5 V aurait un seuil haut à 3,5 V et ne conviendrait pas. Si cette référence ne se trouve pas, un `SN74HCT125` en SOIC-14 fait le travail et est très largement stocké.
- **J3, `FH34SRJ-10S-0.5SH(50)`** — connecteur FPC 10 voies au pas de 0,5 mm. Vérifier le sens de contact (dessus/dessous) contre la nappe réelle du LS027B7DH01, c'est le genre d'erreur qui ne se voit qu'au montage.
- **LS1, `7BB-20-6L0`** — les suffixes de la série 7BB de Murata varient. Choisir le disque par sa **fréquence de résonance à l'air libre** plutôt que par sa référence, puis recentrer la loi sonore dessus.

## Choix qui méritent une explication

**LSM6DS3TR-C plutôt qu'un MPU-6050.** Le MPU-6050 de la variante A est un composant ancien, en fin de vie chez TDK, qui n'existe plus vraiment qu'en module. Sur un PCB maison il faut une pièce distribuée : le LSM6DS3TR-C est largement stocké et bon marché. Les IMU techniquement supérieurs — ICM-42688-P, série Bosch BMI — étaient annoncés à 22 et 26 semaines de délai au deuxième trimestre 2026, ce qui est disqualifiant pour un projet censé être reconstructible par n'importe qui. **À revérifier au moment de commander** : c'est exactement ce que TrustedParts sait dire.

**Deux rails d'alimentation.** Un élévateur vers 5 V (U6) puis un régulateur linéaire vers 3,3 V (U7). Le rail 5 V est de toute façon nécessaire pour le tampon audio et pour le panneau Sharp, dont la caractérisation est faite à 5 V. Le rendement du linéaire est médiocre, mais à ~55 mA cela reste acceptable et le montage est beaucoup plus simple qu'un convertisseur abaisseur-élévateur.

**MCP73831 plutôt qu'un TP4056.** Le TP4056 est excellent et coûte trois fois moins cher, mais il n'existe en pratique que chez LCSC et sous forme de module. Le MCP73831 est chez tous les distributeurs agréés. Sa résistance de réglage `R5` de 2 kΩ fixe le courant de charge à 500 mA.

**Pas de témoin lumineux.** Ce n'est pas un oubli : le MS5611 est sensible à la lumière, sa puce étant exposée sous le capot métallique. Aucune source lumineuse ne doit se trouver près de lui.

## Ce qui manque encore

La batterie (`BT1` de la variante A) n'apparaît pas dans la nomenclature au composant : un accumulateur lithium-polymère se choisit sur son encombrement et sa protection intégrée, pas sur une référence fabricant, et l'expédition en est réglementée. À acheter localement, **avec circuit de protection intégré**.

Les passifs listés couvrent le découplage, le diviseur de mesure batterie, les tirages I2C et les tirages au moins CC de l'USB-C. La liste définitive ne sera figée qu'avec le schéma KiCad (étape P7), et c'est lui qui devra ensuite engendrer cette nomenclature automatiquement plutôt que de la maintenir à la main.
