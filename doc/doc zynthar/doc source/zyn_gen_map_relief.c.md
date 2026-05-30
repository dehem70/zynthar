# 🏔️ Documentation Technique : `zyn_gen_map_relief.c`

Ce module implémente le moteur de génération macro-procédural du relief de Zynthar. À partir de masques de Voronoi, de bruit fractal et d'automates cellulaires, il sculpte et stabilise l'ossature géométrique globale de la carte ($1000.96 \times 500.224\text{ km}$).

## 📌 Choix d'Architecture & Optimisations

- **Alignement d'Axe Longitudinal :** Aligné sur le repère horizontal ($X, Z$) conforme aux conventions d'affichage de _Babylon.js_.
    
- **Zéro Dérive Arithmétique :** Les calculs intermédiaires s'exécutent en virgule flottante de haute précision (`float`), tandis que l'unique arrondi final convertit les altitudes en décimètres (`int16_t`) pour le stockage packagé en base SQLite3.
    
- **Localité Séquentielle (RAM) :** L'allocation de la grille s'effectue en un bloc mémoire contigu afin de maximiser l'efficacité du cache CPU lors des balayages de pixels.
    

## 🛠️ Gestion de la Mémoire (Allocation Contiguë)

### `zyn_gen_map_relief_alloc`

C

```
MacroChunk* zyn_gen_map_relief_alloc(int32_t width_x, int32_t depth_z);
```

- **Description :** Alloue un espace mémoire contigu sur le tas pour stocker la grille de `MacroChunk`. L'utilisation de `calloc` garantit la réinitialisation de toutes les structures à zéro.
    

## 🗺️ Algorithme du Masque de Voronoi (Génération des Germes)

### `zyn_gen_map_relief_voronoi`

C

```
float* zyn_gen_map_relief_voronoi(int32_t width_x, int32_t depth_z, int32_t num_islands);
```

- **Description :** Génère un masque de vagues et d'îles normalisé entre $0.0f$ et $1.0f$.
    
- **Déterminisme local :** Emploie un Générateur Congruentiel Linéaire (LCG) privé pour distribuer spatialement les centres des îles (`IslandSeed`) de façon strictement identique à chaque exécution.
    

#### 📊 Logique d'Évaluation de Distance

Le calcul détermine le plus proche voisin à l'aide de la distance euclidienne mise au carré (pour éviter l'appel système coûteux à `sqrtf` dans la boucle la plus interne). La racine n'est calculée qu'une fois la distance minimale trouvée :

```
[Pour chaque coordonnée globale (X, Z)]
                  │
                  ▼
  [min_dist_carre = 99999999.0f]
                  │
                  ▼
 ┌───► [Boucle sur les i germes d'îles] ───────────────┐
 │       │                                             │
 │       ▼                                             │
 │   [dx = seed.x - x]                                 │
 │   [dz = seed.z - z]                                 │
 │   [dist_carre = dx² + dz²]                          │
 │       │                                             │
 │       ▼                                             │
 │   (dist_carre < min_dist_carre) ?                   │
 │         ├── Oui ──► [min_dist_carre = dist_carre]   │
 │         └── Non ──► [Continuer]                     │
 │       │                                             │
 └───────┴─────────────────────────────────────────────┘
                  │
                  ▼
     [min_dist = sqrtf(min_dist_carre)]
     [Valeur = 1.0f - (min_dist / max_dist)]
     [Bornage de sécurité : fmaxf(0.0f, Valeur)]
```

## 🌊 Pipeline de Fusion de l'Archipel (Calibrage de la Mer)

### `zyn_gen_map_relief_archipelago`

C

```
void zyn_gen_map_relief_archipelago(MacroChunk* map, int32_t width_x, int32_t depth_z, int32_t num_islands, float max_sea_percentage);
```

Cette fonction orchestre la morphologie finale du monde. Elle combine le bruit fractal de Perlin, le _Domain Warping_ (distorsion de coordonnées pour créer des côtes déchiquetées) et le masque de Voronoi.

#### 📊 Algorithme d'Étalement et Correction Asymétrique

Pour garantir que le relief respecte rigoureusement les limites imposées par la configuration (`ZYN_WORLD_Y_MIN` et `ZYN_WORLD_Y_MAX`) sans écraser le relief, la fonction applique une mise à l'échelle asymétrique par morceaux:

```
             [Calcul du relief brut pour chaque case]
    [relief_brut = (bruit * 0.5f) + (v_mask² * 1.4f) - (ocean * 0.8f)]
                                │
                                ▼
         [Tri rapide (qsort) de la copie du tableau]
                                │
                                ▼
       [Détermination de la valeur du niveau de la mer]
           [pivot = hauteurs_triees[total * sea_pct]]
                                │
                                ▼
          [Balayage de recherche des extrêmes relatifs]
   - max_brut_terre = valeur_max_positive(alt - pivot)
   - min_brut_mer   = valeur_max_negative(alt - pivot)
                                │
                                ▼
       [Calcul des coefficients d'étirement linéaires]
   - coef_positif = ZYN_WORLD_Y_MAX / max_brut_terre
   - coef_negatif = ZYN_WORLD_Y_MIN / min_brut_mer
                                │
                                ▼
 ┌──────────────────────────────┴──────────────────────────────┐
 ▼                                                             ▼
[Si alt_relative > 0.0f (Terre)]              [Si alt_relative < 0.0f (Mer)]
 alt_m = alt_relative * coef_positif           alt_m = -(|alt_relative| * coef_negatif)
 └──────────────────────────────┬──────────────────────────────┘
                                │
                                ▼
         [Bornage de sécurité contre le dépassement]
      [alt_dm = (int16_t)roundf(alt_clamped_m * 10.0f)]
```

## 🧮 Lissage des Côtes par Automate Cellulaire

### `zyn_gen_map_relief_smooth_coastlines`

C

```
void zyn_gen_map_relief_smooth_coastlines(MacroChunk* map, int32_t width_x, int32_t depth_z, int32_t iterations);
```

- **Description :** Élimine les bruits de génération isolés (un bloc de terre au milieu de l'eau ou inversement) sur les lignes de rivage.
    
- **Voisinage de Moore :** Analyse les 8 voisins entourant la cellule courante.
    
- **Optimisation RAM & CPU :** Utilise un tableau binaire compact (`1` pour la terre, `0` pour la mer), réduisant l'allocation à 1 octet par case. Le déroulage complet de la somme évite les boucles imbriquées imbriquées.
    

#### 📊 Règles de Transition de l'Automate

À chaque itération, la transition d'état s'applique selon les conditions suivantes :

```
                      [Cellule Actuelle]
                              │
               ┌──────────────┴──────────────┐
               ▼                             ▼
       [État = 1 (Terre)]             [État = 0 (Mer)]
               │                             │
       (voisins_terre < 4) ?         (voisins_terre >= 5) ?
         ├── Oui ──► [Mer (0)]         ├── Oui ──► [Terre (1)]
         └── Non ──► [Terre (1)]       └── Non ──► [Mer (0)]
```

> ⚠️ **Préservation de la topographie :** Lors de la réapplication du masque lissé sur le relief réel, si une cellule de terre ferme (`1`) a vu son altitude passer sous la mer suite au lissage, la fonction force l'altitude à une valeur positive minimale de $+1\text{ dm}$ ($+10\text{ cm}$). Inversement, une cellule d'eau lissée est bridée à un maximum de $-1\text{ dm}$ ($-10\text{ cm}$).