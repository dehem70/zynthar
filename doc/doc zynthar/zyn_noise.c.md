Ce module implémente le moteur de génération mathématique de bruit procédural (2D et 3D) pour le projet Zynthar. Il utilise un algorithme de bruit de gradient (type Perlin) déterministe pour sculpter le relief et définir les biomes sans stocker de cubes bruts en base de données.

## 📌 Spécifications d'Architecture Axes & Alignement

- **Axes Horizontaux :** $X$ et $Z$
    
- **Axe Vertical (Hauteur) :** $Y$ (Alignement direct avec le moteur de rendu _Babylon.js_)
    
- **Structure Interne :** Table de permutation statique de 512 octets pour éviter les calculs de modulo `(%)` lents lors des accès indexés fréquents.
    

## 🛠️ Structures de Données & Variables Globales

### Table de Permutation Interne

C

```
static uint8_t p[512];
```

- **Rôle :** Stocke la table de hachage pseudo-aléatoire dupliquée. La taille de 512 permet de réaliser des accès de type `p[X + Y]` sans risque de dépassement de mémoire (Buffer Overflow) et sans calcul de reste de division.
    

## ⚙️ Fonctions d'Initialisation et Utilitaires

### `zyn_noise_init`

C

```
void zyn_noise_init(uint32_t seed_value);
```

- **Description :** Initialise la graine (seed) du monde à l'aide d'un Générateur Pseudo-Aléatoire Déterministe (LCG - _Linear Congruential Generator_). Elle effectue un mélange de Fisher-Yates (Shuffle) sur les 256 premières valeurs, puis duplique le tableau sur les 256 octets suivants.
    

#### 📊 Algorithme de Flux

```
[Entrée : seed_value]
         │
         ▼
[Remplir base_p de 0 à 255]
         │
         ▼
 ┌───► [Boucle i de 255 à 1] ──────────────────────────────────┐
 │       │                                                     │
 │       ▼                                                     │
 │   [Calcul LCG : next_random = next_random * LCG_M + LCG_A]  |
 │       │                                                     │
 │       ▼                                                     │
 │   [Index cible = (next_random / 65536) % (i + 1)]           │
 │       │                                                     │
 │       ▼                                                     │
 │   [Permuter base_p[i] et base_p[target_index]]              │
 │       │                                                     │
 └───────┴─────────────────────────────────────────────────────┘
         │
         ▼
[Dupliquer base_p dans p[512] via un masque binaire (i & 255)]
         │
         ▼
     [Fin Init]
```

### `zyn_grad2d` & `zyn_grad3d`

C

```
static inline float zyn_grad2d(uint8_t hash, float x, float z);
static inline float zyn_grad3d(uint8_t hash, float x, float z, float y);
```

- **Description :** Fonctions _inline_ calculant le produit scalaire entre les vecteurs de distance fractionnaire et les gradients pseudo-aléatoires issus de la table de hachage. `zyn_grad3d` utilise les 12 arêtes d'un cube unité.
    

## 🏔️ Générateurs de Bruit Fondamentaux

### `zyn_noise2d`

C

```
float zyn_noise2d(float x, float z);
```

- **Description :** Calcule une valeur de bruit de gradient 2D standard comprise entre $-1.0$ et $1.0$ sur le plan horizontal $(X, Z)$. Idéal pour les calculs macroscopiques (température, humidité globale).
    

#### 📊 Logique d'Interpolation 2D

```
(X, Z+1) [ab] ─────────── [bb] (X+1, Z+1)
           │               │
           │       •       │  <-- Point (xf, zf) à interpoler
           │               │
  (X, Z) [aa] ─────────── [ba] (X+1, Z)
           ^               ^
        zyn_fade(xf) et zyn_fade(zf) appliqués pour le lissage en S
```

### `zyn_noise3d`

C

```
float zyn_noise3d(float x, float z, float y);
```

- **Description :** Génère un bruit tridimensionnel en fusionnant par triple interpolation linéaire (Trilinear Lerp) les contributions des 8 sommets du cube virtuel englobant le point donné. **Attention à l'ordre des paramètres :** $Y$ représente la hauteur et se place en dernier argument pour correspondre à la logique de la base de données.
    

#### 📊 Algorithme Mathématique 3D

```
                [Entrée : Coordonnées x, z, y]
                              │
                              ▼
                [Calcul des parties entières (X, Z, Y)]
                [Calcul des fractions (xf, zf, yf)]
                              │
                              ▼
                [Application de la courbe de lissage]
                     u = zyn_fade(xf)
                     v = zyn_fade(zf)
                     w = zyn_fade(yf)
                              │
                              ▼
                [Hachage des 8 sommets du cube via p]
                              │
                              ▼
         ┌────────────────────┴────────────────────┐
         ▼                                         ▼
[Interpolation 2D Face Basse (Y)]         [Interpolation 2D Face Haute (Y + 1)]
  - Lerp sur X (aa -> ba)                   - Lerp sur X (aa+1 -> ba+1)
  - Lerp sur X (ab -> bb)                   - Lerp sur X (ab+1 -> bb+1)
  - Lerp sur Z avec le facteur v            - Lerp sur Z avec le facteur v
         │                                         │
         └────────────────────┬────────────────────┘
                              │
                              ▼
                [Interpolation Finale entre les deux faces]
                     Lerp finale avec le facteur w (Hauteur)
                              │
                              ▼
                       [Retour de la valeur]
```

## 📈 Algorithmes Fractals (Fractional Brownian Motion - FBM)

Ces fonctions superposent plusieurs couches de bruit (octaves) pour créer des reliefs réalistes, des falaises ou des crevasses.

### `zyn_fractal_noise2d` & `zyn_fractal_noise3d`

C

```
float zyn_fractal_noise2d(float x, float z, int32_t octaves, float persistence, float lacunarity);
float zyn_fractal_noise3d(float x, float z, float y, int32_t octaves, float persistence, float lacunarity);
```

- **`octaves` :** Nombre de couches superposées (détails de plus en plus fins).
    
- **`persistence` :** Multiplicateur d'amplitude à chaque octave (généralement $< 1.0$). Diminue l'impact des fréquences élevées.
    
- **`lacunarity` :** Multiplicateur de fréquence à chaque octave (généralement $> 1.0$). Augmente la granularité.
    

#### 📊 Boucle d'accumulation FBM

```
[total = 0, amplitude = 1.0, frequence = 1.0, max_amplitude = 0]
                             │
                             ▼
     ┌───► [Boucle i allant de 0 à octaves - 1] ────────────┐
     │       │                                              │
     │       ▼                                              │
     │   [Valeur = zyn_noise(coordonnées * frequence)]      │
     │   [total += Valeur * amplitude]                      │
     │   [max_amplitude += amplitude]                       │
     │       │                                              │
     │       ▼                                              │
     │   [amplitude *= persistence]                         │
     │   [frequence *= lacunarity]                          │
     │       │                                              │
     └───────┴──────────────────────────────────────────────┘
                             │
                             ▼
               [Retourner total / max_amplitude] 
           (Garantit un signal normalisé entre -1.0 et 1.0)
```
