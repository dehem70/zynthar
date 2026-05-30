# Spécifications Techniques & Persistance : `zynthar.h`

Ce document fait office de référence pour l'implémentation de la **Brique 2 (Persistance)** et des outils de la **Brique 5 (Génération Offline)**. Il détaille les configurations, l'ensemble des constantes mathématiques, les structures de données globales et l'analyse de l'en-tête technique du projet.

## 🗄️ 1. Structure de la Base de Données (SQLite3)

Pour mémoire, la base de données ne stocke aucun voxel brut afin d'éviter l'explosion de la taille du fichier. L'univers s'appuie sur trois bases de données distinctes et le fichier `zyn-world.db` est composé d'une grille de **2 000 × 1 000 Macro-Chunks** (soit 2 millions de lignes).

### Fichiers de Persistance Spécifiés

- `data/zyn-world.db` : Contient le squelette du monde macroscopique (climat, biomes).
    
- `data/zyn-player.db` : Gère l'état, l'authentification et les métadonnées des joueurs.
    
- `data/zyn-delta.db` : Enregistre exclusivement les actions dynamiques (blocs posés/cassés).
    

### Schéma Logique de la Table `MacroChunks`

La structure C `MacroChunk` fait directement écho à la table de persistance locale :

|**Champ SQL**|**Type C**|**Taille**|**Description / Contraintes**|
|---|---|---|---|
|`chunk_x` **(PK)**|`int32_t`|4 octets|Coordonnée macroscopique X (Clé composite).|
|`chunk_z` **(PK)**|`int32_t`|4 octets|Coordonnée macroscopique Z (Clé composite).|
|`elevation_max_dm`|`int16_t`|2 octets|Altitude maximale en **décimètres** (Plage : `-10240` à `+20480`).|
|`temperature_raw`|`uint8_t`|1 octet|Température normalisée brute (0 à 255).|
|`biome`|`uint8_t`|1 octet|Identifiant unique du biome (`ZYN_BIOME_*`).|

> 🔒 **Optimisation Mémoire :** La structure C est alignée à la perfection sur **12 octets** au total. Il n'y a aucun _padding_ invisible injecté par le compilateur, garantissant des performances de lecture binaire directe (_raw stream_) maximales.

## ⚙️ 2. Synthèse des Outils de Conversion (Macros)

Le fichier `zynthar.h` fournit des macros critiques pour convertir les types de données physiques du moteur de jeu vers les formats compressés ou optimisés de la base de données.

### A. Métriques de Distance (Mètres $\leftrightarrow$ Décimètres)

L'élévation est stockée en décimètres (`int16_t`) pour économiser 2 octets par rapport à un `float32`, tout en conservant une précision de 10 cm, alignée sur la taille d'un micro-voxel.

- $$M\_TO\_DM(m) = \text{(int16\_t)}(m \times 10.0f)$$
    
    (Conversion vers le stockage).
    
- $$DM\_TO\_M(dm) = \text{(float)}(dm) / 10.0f$$
    
    (Restitution pour le moteur physique).
    

### B. Métriques Climatiques (Float $[0.0, 1.0]$ $\leftrightarrow$ Uint8 $[0, 255]$)

Normalise et transpose les valeurs de température générées par les bruits mathématiques vers la plage réelle du monde de Zynthar (de **-25°C** à **45°C**).

- **`FLOAT_TO_RAW(f)`** : Évalue la valeur flottante `f` ($[0.0, 1.0]$), la projette sur la plage d'octets `[0, 255]` et applique le décalage thermique de Zynthar.
    
- **`RAW_TO_FLOAT(r)`** : Décompresse la valeur brute `r` (`uint8_t`) pour restituer la température mathématique exacte en jeu.
    

## 📋 3. Liste exhaustive des Constantes et Variables Globales

Voici l'ensemble des définitions préprocesseur (`#define`) déclarées dans le fichier d'en-tête `zynthar.h`, triées par domaine fonctionnel :

### Configurations et Systèmes de Fichiers

- `ZYN_DB_EMPLACEMENT` (`"data/"`) : Répertoire racine de stockage des bases SQLite.
    
- `ZYN_DB_WORLD` (`"zyn-world.db"`) : Fichier de la base de génération du monde.
    
- `ZYN_DB_PLAYER` (`"zyn-player.db"`) : Fichier de la base de données des joueurs.
    
- `ZYN_DB_DELTA` (`"zyn-delta.db"`) : Fichier contenant l'historique des modifications de blocs.
    

### Limites Absolues de l'Univers

- `ZYN_WORLD_X_MAX` (`1000960`) : Longueur totale de la carte en mètres ($1,000.96\text{ km}$).
    
- `ZYN_WORLD_Z_MAX` (`500224`) : Largeur totale de la carte en mètres ($500.224\text{ km}$).
    
- `ZYN_WORLD_Y_MIN` (`-1024`) : Coordonnée d'altitude minimale en mètres (profondeurs).
    
- `ZYN_WORLD_Y_MAX` (`2048`) : Coordonnée d'altitude maximale en mètres (cieux).
    
- `ZYN_SEA_LEVEL` (`0`) : Altitude hydrographique de référence (niveau de la mer).
    
- `ZYN_WORLD_TEMP_MIN` (`-25`) : Température minimale absolue en degrés Celsius.
    
- `ZYN_WORLD_TEMP_MAX` (`45`) : Température maximale absolue en degrés Celsius.
    

### Dimensions Géométriques & Métriques du Moteur

- `ZYN_VOXEL_TO_M` (`0.1f`) : Taille d'une arête de voxel unitaire ($10\text{ cm}$).
    
- `ZYN_MICRO_CHUNK_DIM_VOX` (`256`) : Nombre de voxels constituant le côté d'un Micro-Chunk.
    
- `ZYN_MICRO_CHUNK_SHIFT` (`8`) : Exposant binaire ($2^8 = 256$) employé pour les calculs par décalage de bits au CPU.
    
- `ZYN_MACRO_CHUNK_DIM_M` (`512`) : Longueur d'un côté de Macro-Chunk exprimée en mètres.
    

### Constantes Physiques de Gameplay (Seuils en Voxels)

- `ZYN_SEUIL_MARCHE_AUTO` (`4`) : Obstacles $\le 4$ voxels ($\le 40\text{ cm}$) franchis sans sauter.
    
- `ZYN_SEUIL_SAUT` (`10`) : Obstacles de 5 à 10 voxels ($40\text{ cm}$ à $1\text{ m}$) franchissables d'un saut.
    
- `ZYN_SEUIL_ESCALADE` (`16`) : Obstacles de 11 à 16 voxels ($1\text{ m}$ à $1.6\text{ m}$) nécessitant une escalade haute.
    

### Registre des Identifiants de Biomes (`uint8_t`)

Chaque constante correspond à un identifiant numérique d'environnement exploité par le générateur :

- `ZYN_BIOME_INCONNU` : `0`
    
- `ZYN_BIOME_EAU_PROFONDE` : `1`
    
- `ZYN_BIOME_EAU_COTIERE` : `2`
    
- `ZYN_BIOME_PLAINE` : `3`
    
- `ZYN_BIOME_DESERT` : `4`
    
- `ZYN_BIOME_FORET` : `5`
    
- `ZYN_BIOME_TAIGA` : `6`
    
- `ZYN_BIOME_TOUNDRA` : `7`
    
- `ZYN_BIOME_JUNGLE` : `8`
    
- `ZYN_BIOME_GLACIER` : `9`
    
- `ZYN_BIOME_PLAGE` : `10`
    
- `ZYN_BIOME_MONTAGNE_ROCHEUSE` : `11`
    
- `ZYN_BIOME_PIC_ENNEIGE` : `12`