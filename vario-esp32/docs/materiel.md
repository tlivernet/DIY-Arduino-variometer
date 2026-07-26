# Matériel

## Deux variantes assumées

Servir à la fois « clé en main » et « esprit DIY » demande deux niveaux de montage.

**Variante A — modules.** Carte ESP32-S3 du commerce et modules tout faits, entièrement soudable à la main, montable sur plaque à trous *ou* sur un circuit imprimé simple. C'est la cible du guide de montage sur perfboard, et elle est livrée en premier.

**Variante B — intégrée.** Un seul circuit imprimé deux couches portant un module ESP32-S3-WROOM-1, le baromètre, la centrale inertielle et le chargeur, tous en composants montés en surface, assemblé par JLCPCB. C'est le « produit fini », repoussé en fin de projet.

## Fichiers de nomenclature

Deux fichiers dans [`hardware/bom/`](https://github.com/tlivernet/DIY-Arduino-variometer/tree/master/vario-esp32/hardware/bom) :

- `bom-composants.csv` — composants unitaires de la variante B, **à téléverser sur Octopart, TrustedParts ou FindChips** pour obtenir stock, prix et délais réels ;
- `bom-modules.csv` — modules de la variante A, qui ne se résolvent pas chez les distributeurs agréés et servent de simple liste de courses.

```bash
make bom   # valide et produit les fichiers réduits à téléverser
```

!!! warning "Les références n'ont pas été vérifiées en direct"
    Elles ont été choisies pour être des pièces réelles et couramment distribuées, mais ni le stock, ni le prix, ni le délai, ni même l'exactitude de chaque suffixe n'ont été contrôlés. C'est exactement ce que ces outils savent faire : téléverse le fichier et lis leur réponse. Trois lignes sont d'ailleurs explicitement marquées à vérifier.

## Nomenclature de la variante A

| Poste | Référence | Prix indicatif |
|---|---|---|
| Calculateur | Waveshare ESP32-S3-Zero (23,5 × 18 mm, USB-C natif, 4 Mo de flash) | 5 € |
| Baromètre | module MS5611 / GY-63 | 4 € |
| Centrale inertielle | module 6 axes MPU6050 / MPU6500 (GY-521) | 2 € |
| Écran | Sharp LS027B7DH01 sur carte d'adaptation au pas de 2,54 mm | 20 € |
| Son | piezo large bande + porte logique 5 V | 1 € |
| Commande | trois boutons tactiles sous membrane silicone | 1 € |
| Énergie | batterie LiPo 1S 1500 mAh | 5 € |
| Charge | TP4056 avec protection, ou chargeur intégré à la carte | 1 € |
| Horloge | DS3231SN + pile de sauvegarde | 1,5 € |
| Divers | interrupteur, diviseur de tension, résistances, condensateurs | 1,5 € |
| Circuit imprimé | deux couches, par cinq chez JLCPCB | 1,5 € |
| Boîtier | ~45 g de PETG | 1 € |
| **Total** | | **≈ 43 €** |

À comparer aux 26,85 € de la version de 2014 — avec un écran à 2 € — et aux 250 à 400 € d'un vario du commerce. L'écran Sharp représente à lui seul près de la moitié du budget ; l'abstraction d'affichage permettra une variante OLED aux alentours de 27 € pour qui veut réduire.

## Pourquoi ces choix

### Le Sharp Memory LCD plutôt qu'un e-paper

L'e-paper est tentant, et il est éprouvé : GNUVario-E vole avec un écran e-paper de 2,9". Cela fonctionne parce qu'en vario **l'information temps réel passe par le son**, pas par l'écran — celui-ci n'affiche que de l'altitude et du chronomètre, qui évoluent lentement.

Deux limites réelles ont fait pencher la balance :

- **le froid.** Un e-paper standard est spécifié de 0 à 40 °C ; sous 0 °C le rafraîchissement ralentit fortement et le ghosting augmente. Or à 3000 m au printemps on y est couramment. Les variantes larges températures existent, mais leur rafraîchissement « rapide » est de 3,5 s ;
- **le rafraîchissement**, ~0,3 s en partiel et ~2 s en complet, avec du ghosting qui s'accumule.

Le Sharp Memory LCD offre la même lisibilité en plein soleil et une consommation quasi nulle à l'affichage figé, mais avec un rafraîchissement rapide, aucun ghosting, et un fonctionnement par grand froid.

### Le MS5611 plutôt qu'un capteur plus récent

C'est la référence de la communauté du vol libre, et l'auteur du projet har-in-air documente pourquoi : le BMP280 présente des **pics de bruit de forte amplitude** rédhibitoires en vario, même à variance globale comparable.

!!! note "À réévaluer"
    Le MS5611 est une puce de 2010, et les modules « GY-63 » sont massivement réétiquetés — on ne sait pas toujours quelle puce on reçoit. Le **BMP390** est meilleur sur le bruit, la vitesse et la consommation. Le baromètre est donc placé derrière une interface pour que le remplacer ne soit qu'une affaire de pilote.

### Trois boutons plutôt qu'un encodeur rotatif

Décision révisée par rapport à 2017. Avec des gants de 3 mm on ne distingue pas les crans d'un encodeur, et son axe est le pire chemin d'entrée d'eau et de poussière de tout l'appareil. Tous les instruments de vol libre du commerce utilisent des boutons. Le menu d'origine se réduisait déjà à haut / bas / valider, la transposition est directe.

Bénéfice annexe : plus de décodage en quadrature par interruption, donc une logique de boutons purement testable.

### Une horloge dédiée reste nécessaire

Le `RTC_SLOW_CLK` de l'ESP32-S3 est cadencé par défaut sur un oscillateur RC interne à 150 kHz précis à ±5 %, soit **environ une heure de dérive par jour**, et la plupart des petites cartes S3 ne montent pas le quartz de 32,768 kHz. Un DS3231SN à ±2 ppm règle la question pour 1,5 €. Sur un circuit imprimé maison, poser un quartz de 32 kHz permettrait de s'en passer.

## Pièges à traiter dès la conception

Ce sont eux qui décident si un montage vole ou non.

!!! danger "Le MS5611 est sensible à la lumière"
    Sa puce est exposée sous le capot métallique : les photons faussent la mesure de pression. Il doit être en chambre borgne, recouvert de mousse noire à cellules ouvertes. Ne pas placer de témoin lumineux à proximité.

!!! danger "Le baromètre ne doit pas voir la pression dynamique"
    Chambre isolée reliée à l'intérieur du boîtier par un évent en labyrinthe, jamais directement à l'air extérieur ni face au vent relatif. Sans cela l'écart-type réel en vol est de 0,5 m et non de 0,15 m, et tout le réglage du filtre s'écroule.

**Découpler le baromètre du piezo.** Les vibrations acoustiques se lisent dans la pression. Éloigner les deux et ne pas partager la même paroi.

**Le panneau Sharp nu se connecte en nappe FPC de 0,5 mm à 10 voies**, infaisable sur plaque à trous. D'où la carte d'adaptation à embase au pas de 2,54 mm en variante A, et l'embase FPC réservée à la variante B.

**L'écran Sharp exige un basculement de polarité VCOM à environ 1 Hz**, sous peine de dégradation permanente du cristal liquide. Il se fait par un minuteur périodique et non par un canal PWM : sur l'ESP32-S3, la fréquence minimale d'un canal LEDC reste très au-dessus du 1 Hz voulu.

**Ne pas mettre de vitre brillante devant l'écran.** La surface réfléchissante mate du Sharp est précisément ce qui le rend lisible au soleil ; un plexiglas poli lui ferait perdre son avantage. Casquette et lèvre de bezel, éventuellement un film antireflet.

**Imprimer en PETG, pas en PLA.** Un boîtier noir au soleil au déchaussement dépasse facilement 60 °C, température à laquelle le PLA ramollit.

**Prévoir un tampon 5 V pour le son.** Le `toneAC` de l'AVR délivrait 10 V crête à crête aux bornes du piezo. Deux sorties ESP32 en push-pull n'en donnent que 6,6, soit environ 3,6 dB de moins — sur le critère le plus important d'un vario, être audible dans le bruit du vent. Une porte logique alimentée en 5 V rétablit les 10 V pour 0,15 € et 2 mm². Elle doit impérativement être de la famille **HCT**, à seuils TTL : un `74HCT2G34` en 5 V a un seuil haut à 2 V, donc une sortie 3,3 V de l'ESP32 le commande sans problème. Un `74LVC` en 5 V aurait un seuil haut à 3,5 V et ne conviendrait pas. Repli très largement stocké si besoin : `SN74HCT125` en SOIC-14.

**Mesurer la batterie sur ADC1 uniquement.** L'ADC2 de l'ESP32 est inutilisable quand le Wi-Fi est actif. Diviseur 2:1 et lecture calibrée par les eFuse — le `4.89 * analogRead()` de l'AVR n'est pas transposable. Mieux encore, une jauge de charge dédiée supprime le problème et sa calibration.
