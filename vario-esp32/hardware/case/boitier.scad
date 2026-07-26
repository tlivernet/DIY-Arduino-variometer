// Boîtier paramétrique du variomètre ESP32 — étape P8.
//
// Modèle en deux coquilles (fond + capot) tenues par quatre vis M2 logées
// dans des OREILLES EXTERNES. Ce choix d'oreilles externes n'est pas
// esthétique : l'écran Sharp (42,8 mm de large) et le PCB visé (46 mm)
// remplissent toute la façade, il ne reste aucune marge intérieure où loger
// une colonne de vis sans mordre l'écran. Les colonnes internes ont été
// écartées pour cette raison ; voir docs/boitier.md.
//
// Empilement, du fond vers la façade :
//   fond → accu LiPo → PCB (composants vers le bas) → écran → capot.
//
// Rendu et export :
//   openscad -o boitier-fond.stl  -D 'piece="fond"'  boitier.scad
//   openscad -o boitier-capot.stl -D 'piece="capot"' boitier.scad
//   openscad -o apercu.png        -D 'piece="apercu"' --render boitier.scad
// La cible `make case` (voir Makefile) enchaîne les trois.
//
// Cotes dérivées de la fiche technique du Sharp LS027B7DH01 et de la
// nomenclature de la variante B (hardware/bom/bom-composants.csv). Elles sont
// à confronter au matériel réel avant impression : voir docs/boitier.md.
// Unité : millimètre.

// ---------------------------------------------------------------------------
// Paramètres — à ajuster ici, jamais dans le corps du modèle.
// ---------------------------------------------------------------------------

// Quelle pièce produire : "fond", "capot", "apercu" (les deux, écartées).
piece = "apercu";

// --- Écran Sharp LS027B7DH01 ---------------------------------------------
// Contour du module nu (fiche technique, tolérance mécanique incluse).
ecran_l   = 42.80;   // largeur du contour de verre
ecran_h   = 61.50;   // hauteur du contour de verre
ecran_ep  = 1.30;    // épaisseur du verre + FPC replié
// Fenêtre visible (aire active), centrée en X, décalée en Y sur le verre.
actif_l   = 34.56;   // aire active horizontale (400 px @ 0,0864 mm)
actif_h   = 57.60;   // aire active verticale
actif_dx  = 4.12;    // décalage de l'aire active depuis le coin du verre
actif_dy  = 1.95;

// --- Carte principale (variante B, PCB intégré) --------------------------
// Le PCB n'existe pas encore (P7) ; ces cotes sont un objectif de conception
// que le tracé KiCad devra respecter, pas une mesure.
pcb_l     = 46.00;
pcb_h     = 66.00;
pcb_ep    = 1.60;
garde_sous_pcb = 3.50;   // hauteur des composants sous le PCB

// --- Accumulateur --------------------------------------------------------
// LiPo 1S à plat au fond. À confronter à l'accu réellement acheté.
accu_ep   = 6.00;

// --- Parois et assemblage ------------------------------------------------
paroi     = 2.00;
fond_ep   = 1.60;    // épaisseur du fond et du dessus
jeu       = 0.30;    // jeu d'ajustement autour des cartes (par côté)
rayon_ext = 3.00;    // arrondi des arêtes verticales extérieures

// --- Vis d'assemblage M2, dans les oreilles externes ---------------------
vis_libre  = 2.30;   // perçage passant (capot)
vis_taraud = 1.70;   // avant-trou à visser (fond)
oreille_d  = 6.50;   // diamètre des oreilles
oreille_dy = 0.55;   // position relative des oreilles le long des grands côtés
                     // (0 = centre, ±0,5 = coins) — décalées vers l'extérieur

// --- Ouvertures de façade ------------------------------------------------
bouton_d     = 4.00;   // perçage des poussoirs de façade
bouton_ecart = 12.00;  // entraxe des trois boutons
piezo_d      = 8.00;   // grille sonore : diamètre du motif
usb_l        = 9.50;   // fenêtre USB-C (largeur)
usb_h        = 3.60;   // fenêtre USB-C (hauteur)

$fn = 48;

// ---------------------------------------------------------------------------
// Cotes dérivées — recalculées, ne pas éditer.
// ---------------------------------------------------------------------------

// La cavité contient le plus large / le plus haut des deux (PCB, écran).
cavite_l = max(pcb_l, ecran_l) + 2 * jeu;
cavite_h = max(pcb_h, ecran_h) + 2 * jeu;
// Profondeur intérieure : tout l'empilement.
cavite_p = garde_sous_pcb + pcb_ep + ecran_ep + accu_ep + jeu;

boitier_l = cavite_l + 2 * paroi;
boitier_h = cavite_h + 2 * paroi;

// Oreilles : centre placé juste à l'extérieur de la paroi, sur les grands côtés.
oreille_x = boitier_l / 2 + oreille_d / 2 - 1.0;   // chevauche la paroi de 1 mm
oreille_y = cavite_h * oreille_dy;

// Cote de la fenêtre écran par rapport au centre (écran centré au montage).
fenetre_dx = (actif_dx + actif_l / 2) - ecran_l / 2;
fenetre_dy = (actif_dy + actif_h / 2) - ecran_h / 2;

// ---------------------------------------------------------------------------
// Modules
// ---------------------------------------------------------------------------

// Prisme à arêtes verticales arrondies.
module prisme_arrondi(l, h, p, r) {
    linear_extrude(height = p)
        offset(r = r) offset(delta = -r)
            square([l, h], center = true);
}

// Position des quatre oreilles, pour répliquer un enfant.
module aux_oreilles() {
    for (sx = [-1, 1], sy = [-1, 1])
        translate([sx * oreille_x, sy * oreille_y, 0]) children();
}

// Contour extérieur incluant les oreilles, sur une hauteur donnée.
module contour_avec_oreilles(hauteur) {
    union() {
        prisme_arrondi(boitier_l, boitier_h, hauteur, rayon_ext);
        aux_oreilles()
            cylinder(d = oreille_d, h = hauteur);
    }
}

// --- Fond : cavité, oreilles taraudées, fenêtre USB ----------------------
module coque_fond() {
    hauteur = cavite_p + fond_ep;
    difference() {
        contour_avec_oreilles(hauteur);

        // Cavité ouverte sur le dessus.
        translate([0, 0, fond_ep])
            linear_extrude(height = cavite_p + 1)
                square([cavite_l, cavite_h], center = true);

        // Avant-trous des vis dans les oreilles.
        aux_oreilles()
            translate([0, 0, -0.5])
                cylinder(d = vis_taraud, h = hauteur + 1);

        // Fenêtre USB-C au ras du fond, sur le petit côté bas.
        translate([0, -boitier_h / 2,
                   fond_ep + garde_sous_pcb + pcb_ep / 2])
            rotate([90, 0, 0])
                linear_extrude(height = paroi * 3, center = true)
                    offset(r = 0.6) square([usb_l, usb_h], center = true);
    }
}

// --- Capot : fenêtre écran, boutons, grille sonore, oreilles percées -----
module coque_capot() {
    difference() {
        contour_avec_oreilles(fond_ep);

        // Fenêtre de l'aire active de l'écran.
        translate([fenetre_dx, fenetre_dy, -0.5])
            linear_extrude(height = fond_ep + 1)
                offset(r = 0.5) square([actif_l, actif_h], center = true);

        // Trois boutons alignés près du bord bas.
        for (i = [-1, 0, 1])
            translate([i * bouton_ecart,
                       -boitier_h / 2 + paroi + bouton_d, -0.5])
                cylinder(d = bouton_d, h = fond_ep + 1);

        // Grille sonore : couronne de perforations près du bord haut.
        translate([0, boitier_h / 2 - paroi - piezo_d, 0])
            for (a = [0 : 60 : 359])
                translate([piezo_d / 3 * cos(a), piezo_d / 3 * sin(a), -0.5])
                    cylinder(d = 1.4, h = fond_ep + 1);

        // Perçages passants des vis dans les oreilles.
        aux_oreilles()
            translate([0, 0, -0.5])
                cylinder(d = vis_libre, h = fond_ep + 1);
    }
}

// ---------------------------------------------------------------------------
// Sélection de la pièce à rendre
// ---------------------------------------------------------------------------

if (piece == "fond")
    coque_fond();
else if (piece == "capot")
    coque_capot();
else {
    // Aperçu : les deux coquilles écartées, capot retourné à côté.
    translate([-boitier_l / 2 - oreille_d - 4, 0, 0]) coque_fond();
    translate([ boitier_l / 2 + oreille_d + 4, 0, fond_ep])
        rotate([180, 0, 0]) coque_capot();
}
