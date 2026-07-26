# Algorithmes

## Estimation de la vitesse verticale

### Le principe à saisir en premier

La boucle de correction barométrique est **délibérément lente** — constante de temps d'environ 3,3 s. Lue naïvement, cette valeur suggère un filtre plus lent que la moyenne glissante de 0,8 s du montage de 2017. C'est faux, et comprendre pourquoi est indispensable avant de toucher au moindre réglage.

**L'accélération est une entrée de commande, pas une mesure.** L'état de vitesse répond à une accélération réelle par intégration pure, sans aucun retard. La faible bande passante de correction achète le faible bruit ; l'action directe achète la rapidité. On obtient les deux simultanément, ce que le filtre purement barométrique ne pouvait structurellement pas faire.

| | 2017 | Refonte |
|---|---|---|
| Latence | ~0,8 s | ~0,15 s |
| Bruit affiché au repos | 0,3 à 0,4 m/s | < 0,1 m/s |
| Seuil de montée utilisable | 0,3 m/s (sous le bruit) | 0,2 m/s |

Un test compare directement les deux filtres sur la même rampe : le Kalman atteint 90 % d'un échelon de 2 m/s en moins de 250 ms, plus de trois fois plus vite que la cascade de moyennes glissantes.

### Filtre de Kalman à trois états

État : **[altitude, vitesse verticale, biais d'accélération verticale]**.

Le troisième état absorbe non seulement le décalage propre du capteur mais aussi **l'erreur d'assiette résiduelle** : ce n'est donc pas une caractéristique du composant, et il doit pouvoir évoluer lentement.

| Paramètre | Valeur | Signification |
|---|---|---|
| Densité spectrale d'accélération | 2,0·10⁻³ m²s⁻⁴/Hz | équivaut à ~0,31 m/s² d'erreur à 96 Hz, dominée par l'assiette et non par le capteur |
| Marche aléatoire du biais | 2,5·10⁻⁵ (m/s²)²/s | 0,05 m/s² sur 100 s : assez lent pour ne pas courir après les virages, assez rapide pour suivre la dérive thermique |
| Écart-type de l'altitude | 0,20 m en vol, 0,12 m sur table | **pas** la résolution du capteur : le MS5611 donne 10 cm, mais la microturbulence réelle et l'aérodynamique de la prise de pression ajoutent 1 à 3 Pa de fluctuation authentique |

Les mesures aberrantes sont **adoucies** plutôt que rejetées : au-delà de trois écarts-types, la variance de mesure est gonflée. Un rejet franc créerait des blocages. Un test vérifie qu'un pic isolé de 5 m ne fait pas sauter la vitesse verticale.

Une garde de divergence surveille l'écart persistant avec le baromètre brut et réinitialise sur la mesure au bout de deux secondes. Combinée aux contrôles de finitude, elle rend impossible le défaut n°3 de 2017 : un NaN qui tuait silencieusement écran et son jusqu'au redémarrage.

### Trois sorties, pas une

Les usages diffèrent, donc les lissages aussi :

- **son** — constante de temps 0,15 s ; le retard gêne plus que le tremblement de hauteur ;
- **affichage** — 0,60 s, pour des chiffres stables ;
- **statistiques** — comme l'affichage, pour ne pas enregistrer des pics de bruit comme records.

## Estimation d'assiette

### Une simplification qui change tout

On n'a besoin que de la **direction du bas terrestre** exprimée dans le repère du boîtier, soit une grandeur à deux degrés de liberté. Le cap est de toute façon inobservable avec six axes, et il est ici inutile : tourner autour de la verticale terrestre ne change pas les coordonnées de cette verticale.

Donc pas de quaternion, pas de magnétomètre — ce qui évite au passage le MPU9250 en fin de vie —, pas de gestion de lacet. Un seul vecteur unitaire à suivre.

### Le problème central : la force centrifuge

En virage coordonné à l'inclinaison φ, l'accéléromètre mesure le facteur de charge, de module g/cos(φ), aligné sur le bas de **l'aile** et non sur le bas terrestre. Un filtre de Mahony naïf converge donc vers « inclinaison nulle » et fabrique une accélération verticale parasite de g·(sec φ − 1), soit **1,52 m/s² à 30° d'inclinaison** — de quoi faire dériver la vitesse verticale de 1,5 m/s par seconde. Et c'est précisément la situation dans laquelle un parapentiste passe son temps.

Trois lignes de défense, par ordre d'importance :

1. **Gains gyro-dominants.** Le recalage par l'accéléromètre agit à l'échelle de 15 s. Un virage de 25 s est donc suivi par le gyromètre, lui insensible à la force centrifuge.
2. **Compensation centrifuge à vitesse-bras fixe.** C'est l'astuce métier qui permet de s'en sortir sans GPS : **un parapente vole à une vitesse de calage quasi constante**, 9 à 11 m/s. Poser 10 m/s donne la force centrifuge à ±15 % près. Le réglage est exposé — une aile de vitesse met 14, un biplace 11.
3. **Fenêtre d'acceptation** sur le module du facteur de charge, pour écarter les transitoires violents plutôt que les virages établis.

### Ce que la compensation apporte réellement

Les jeux de données synthétiques — accéléromètre, gyromètre et pression dérivés d'une même vérité terrain — donnent un résultat plus nuancé que le récit habituel. Deux régimes distincts :

**Aux faibles inclinaisons, elle évite la corruption.** À 15°, le facteur de charge ne vaut que 10,15 m/s², soit 0,34 de plus que g : la fenêtre d'acceptation le laisse passer. Sans compensation, l'accéléromètre corrompt donc activement l'assiette et le filtre converge vers une erreur de **8,4°**. Avec, l'erreur est nulle. C'est le régime où la fenêtre ne protège pas.

**Aux fortes inclinaisons, elle garde l'accéléromètre utilisable.** Au-delà d'environ 23°, le facteur de charge sort de la fenêtre et, sans compensation, **plus aucun échantillon** ne sert au recalage : le filtre tourne en boucle ouverte sur le gyromètre pendant toute la spirale, et le biais gyrométrique dérive sans que rien ne le rattrape. Avec compensation, le taux d'acceptation reste à 100 % au lieu de 0 %.

Des tests verrouillent les deux régimes, de sorte que la compensation ne puisse pas être retirée silencieusement.

### Convention de signe

À l'arrêt, l'accéléromètre mesure l'accélération propre, soit `accel · bas = −g`. D'où :

```
a_verticale = −(accel · bas) − g
```

Vérification : à l'arrêt, `−(−g) − g = 0`. C'est là que tout le monde perd une journée, donc c'est écrit.

## Démarrage

### Alignement à l'arrêt

Pendant deux secondes, l'appareil moyenne l'accéléromètre et le gyromètre, en rejetant la fenêtre si l'appareil bouge. Comme le bas estimé est *défini* comme la direction moyenne mesurée, l'accélération verticale calculée est **identiquement nulle** dans l'assiette de démarrage : le biais du filtre part donc juste par construction, et n'a plus qu'à suivre ses variations ultérieures. C'est bien plus sûr que de le laisser converger à l'aveugle.

### Inhibition du son

Seconde ligne de défense, contre les « bruits de canard » à l'allumage documentés sur les projets comparables. Le son est rendu quand le résidu barométrique et la vitesse verticale sont calmes, ou au bout de quatre secondes dans tous les cas — un allumage en voiture ou en télésiège ne doit pas laisser l'appareil muet indéfiniment. Au pire, 5,5 s de silence au total, soit moins que l'écran d'accueil.

## Son

### Deux profils

Le profil **classique** reproduit les lois de 2017 à l'identique, pour retrouver le son connu. Son défaut : la cadence ne varie que de 4,4 à 9,5 bips par seconde sur toute la plage, ce qui porte peu d'information, et un piezo rend très mal sous 1 kHz.

Le profil **moderne** élargit la cadence de 1 à 12 bips par seconde et remonte la bande de fréquence là où un piezo a un bon rendement. C'est ce gradient — un tic-tac épars en ascendance faible, un trille pressant en forte montée — que les pilotes utilisent réellement pour centrer un thermique.

### Ce qui est conservé

**L'amorce continue de 1,35 s** au premier franchissement de seuil, dont la hauteur suit le vario en direct. C'était un effet de bord d'un compteur d'itérations, mais c'est une vraie bonne idée. Exprimée en temps réel, elle tient en une assertion dans un test, indépendamment de la cadence de boucle — contrairement aux « 41 itérations » d'origine, dont la durée changeait silencieusement avec la charge.

### Ce qui est ajouté

Une **hystérésis** sur les seuils. Son absence, combinée à 0,35 m/s de bruit contre un seuil à 0,30, est exactement ce qui faisait bavarder l'ancien appareil au repos. Un test fait osciller le vario autour du seuil dans la bande d'hystérésis et exige un silence total.

### Le défaut « en descente mais ça s'améliore »

L'ancien code calculait la variation de vario sur une seconde et l'utilisait pour la hauteur et la cadence, mais la décision marche/arrêt continuait de tester le vario brut. **La fonction ne marchait donc jamais** comme prévu, et dans le seul cas où elle s'activait — une descente profonde qui s'améliore vite — son effet était pervers : l'alarme de chute passait brusquement à un bip aigu et rapide.

L'intention est physiquement juste : « l'air devant est meilleur » est une vraie technique. Elle sera réimplémentée correctement, mais désactivée par défaut et dotée d'une signature sonore franchement distincte, un signal qui se déclenche en descente étant déroutant s'il peut être confondu avec une montée.
