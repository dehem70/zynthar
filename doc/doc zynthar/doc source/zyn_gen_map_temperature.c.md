# 🌡️ Documentation Technique : `zyn_gen_map_temperature.c`

Ce module gère le calcul et l'encodage de la température globale à l'échelle macroscopique. Il utilise une approche couplée combinant un gradient de latitude (modèle planétaire), du bruit fractal de Perlin, et un refroidissement thermodynamique basé sur l'altitude réelle du relief.

## 📌 Optimisations Matérielles & Algorithmiques

- **Direct Pointer Incrementation (Pointer Chasing) :** Au lieu d'évaluer l'index à chaque itération via `map[z * width_x + x]`, le code utilise un pointeur glissant `chunk++` qui se déplace de case en case de manière contiguë en RAM. Cela réduit drastiquement les calculs d'offsets CPU et maximise le taux de succès du cache de données.
    
- **Branchless Design :** Le calcul évite les structures conditionnelles (`if/else`) lors de l'application de la perte thermique liée à l'altitude en utilisant la fonction mathématique native `fmaxf`. Cela élimine les risques de rupture de pipeline CPU causés par les erreurs de prédiction de branche (_branch misprediction_).
    

## 📐 Modélisation Physique et Formules Mathématiques

Le calcul de la température s'effectue en trois étapes physiques distinctes :

### 1. Le Gradient Latitudinal (Équateur vs Pôles)

Le monde de Zynthar est modélisé avec un Équateur thermique positionné au centre exact de l'axe longitudinal Z, et deux zones polaires aux extrémités nord et sud. La distance relative par rapport au centre est calculée via :

Gradientthermique ​= 1.0f − zcentre​∣z − zcentre​∣​

- Au centre (z=zcentre​), le gradient vaut 1.0 (chaleur maximale).
- Aux bordures (z=0 ou z=depth_z), le gradient vaut 0.0 (froid maximal).

### 2. Le Couplage Atmosphérique (Loi Thermodynamique)

Le modèle intègre le gradient thermique adiabatique de l'atmosphère terrestre réelle, qui induit une perte de température standard de **−0,0065∘C par mètre d'altitude** au-dessus du niveau de la mer :

ΔT=max(0.0f,Altitudem​−Niveau de la mer)×0.0065f

## 📊 Algorithme Général de Flux

```
                 [Début zyn_gen_map_temperature]
                                │
                                ▼
         [Calcul de la ligne médiane z_centre de la carte]
         [Extraction des constantes : -25°C à +45°C]
                                │
                                ▼
 ┌───► [Boucle sur l'axe longitudinal Z] ───────────────────────────┐
 │       │                                                          │
 │       ▼                                                          │
 │   [Calcul du gradient de latitude (0.0 à 1.0)]                   │
 │   [Calcul du pointeur de ligne : chunk = &map[z * width_x]]      │
 │       │                                                          │
 │       ▼                                                          │
 │   ┌───► [Boucle sur l'axe horizontal X] ──────────────────┐      │
 │   │       │                                               │      │
 │   │       ▼                                               │      │
 │   │   [Calcul du bruit fractal 2D local]                  │      │
 │   │       │                                               │      │
 │   │       ▼                                               │      │
 │   │   [Étape 1 : Fusion du Gradient + Bruit]              │      │
 │   │   - base_abstraite = gradient + (bruit * 0.20f)       │      │
 │   │       │                                               │      │
 │   │       ▼                                               │      │
 │   │   [Étape 2 : Projection en °C Réels]                  │      │
 │   │   - Conversion de la base selon la plage (70°C)       │      │
 │   │   - Refroidissement adiabatique : -0.0065°C / mètre   │      │
 │   │   - Clamping physique strict [-25.0°C , 45.0°C]       │      │
 │   │       │                                               │      │
 │   │       ▼                                               │      │
 │   │   [Étape 3 : Normalisation & Encodage]                │      │
 │   │   - Ratio final = (temp_physique - min) / plage       │      │
 │   │   - Encodage dans l'octet : chunk->temperature_raw    │      │
 │   │       │                                               │      │
 │   │       ▼                                               │      │
 │   │   [Incrémentation du pointeur : chunk++]              │      │
 │   │       │                                               │      │
 │   └───────┴───────────────────────────────────────────────┘      │
 │       │                                                          │
 └───────┴──────────────────────────────────────────────────────────┘
                                │
                                ▼
                  [Fin zyn_gen_map_temperature]
```

## 🗄️ Structure de Données d'Écriture (Packing)

La valeur finale convertie est stockée dans la variable membre de la structure `MacroChunk` :

```
chunk->temperature_raw = (uint8_t)roundf(ratio_final * 255.0f);
```
### Table de Correspondance d'Encodage

Grâce à ce packing sur un unique octet (`uint8_t`), la précision thermique obtenue est de ≈0,27∘C par palier, ce qui est largement suffisant pour la gestion macro-procédurale des biomes, tout en divisant par 4 le poids du stockage en base de données par rapport à un `float`.

| **Valeur Brute (uint8_t)** | **Ratio Mathématique** | **Température Réelle Correspondante**           |
| -------------------------- | ---------------------- | ----------------------------------------------- |
| **`0`**                    | 0.0                    | −25,0∘C (Pôle / Haute altitude)                 |
| **`91`**                   | ≈0.357                 | 0,0∘C (Seuil de gel)                            |
| **`127`**                  | ≈0.5                   | +10,0∘C (Climat tempéré)                        |
| **`255`**                  | 1.0                    | +45,0∘C (Équateur / Désert au niveau de la mer) |
