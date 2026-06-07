

# 📑 RÉFÉRENTIEL TECHNIQUE D'I/O & STRUCTURES MÉMOIRE — BRIQUE 2

## 1. Topologie des Identifiants Uniques (Clé Primaire)

L'identifiant unique d'un Macro-Chunk dans toutes les bases SQLite3 est un entier 32 bits sans signe (`uint32_t`) mappé sur une `union` C pour éliminer le coût des décalages de bits lors de l'accès aux coordonnées géométriques du monde de Zynthar :

C

```
#include <stdint.h>

typedef union {
    uint32_t raw_id;
    struct {
        uint16_t x; // Coordonnée Macro X [0..2000]
        uint16_t z; // Coordonnée Macro Z [0..1000]
    } coords;
} MacroChunkID;
```

## 2. Formats des Schémas et Blobs en Base de Données (SQLite3)

Chaque base de données utilise le modificateur `WITHOUT ROWID` indexé sur la clé primaire `id` pour maximiser le débit des requêtes d'accès direct. Les bases de données sont séparées sur le `ramdisk` et unifiées au runtime par le Worker 0 via la commande `ATTACH DATABASE`.

### A. Base du Relief (`main` / `zyn_world.db`)

- **Schéma SQL :** ```sql
    
    CREATE TABLE IF NOT EXISTS macro_chunks (id INTEGER PRIMARY KEY, data BLOB NOT NULL) WITHOUT ROWID;
    
- **Structure du BLOB (4 octets, sans padding, copie binaire brute) :**
    
    C
    
    ```
    typedef struct __attribute__((packed)) {
        uint8_t biome_type;       // ID du biome (ex: 0x01 = Désert, 0x02 = Plaine...)
        uint8_t temperature_raw;  // Température de base [0..255]
        uint8_t humidity_raw;     // Humidité de base [0..255]
        uint8_t max_elevation_dm; // Élévation maximale théorique en décamètres
    } MacroWorldBlob;
    ```
    

### B. Base des Rivières (`rivers` / `zyn_rivers.db`)

- **Schéma SQL :** ```sql
    
    CREATE TABLE IF NOT EXISTS macro_chunks (id INTEGER PRIMARY KEY, data BLOB NOT NULL) WITHOUT ROWID;
    
- **Structure du BLOB (4 octets, alignement 32 bits) :**
    
    C
    
    ```
    typedef struct __attribute__((packed)) {
        uint8_t river_flow;       // Débit du cours d'eau
        uint8_t river_direction;  // Vecteur de direction (angle encodé sur 8 bits)
        uint16_t reserved;        // Padding d'alignement / Variable future
    } MacroRiverBlob;
    ```
    

### C. Base des Modifications (`deltas` / `zyn_deltas.db`)

- **Schéma SQL :** ```sql
    
    CREATE TABLE IF NOT EXISTS micro_deltas (macro_id INTEGER, local_pos INTEGER, matiere INTEGER, PRIMARY KEY(macro_id, local_pos)) WITHOUT ROWID;
    

## 3. Structures Mémoire Partagée (RAM) & Alignement Cache CPU

Toutes les structures de données transitant dans la file d'attente ou le pool de calcul sont alignées de manière stricte sur **64 octets** afin d'éviter le phénomène de _False Sharing_ (concurrence de lignes de cache entre cœurs CPU).

### A. Machine d'États et Structure d'un NanoJob

La file d'attente globale (Ring Buffer) est une table d'états atomique supervisée par le Surveillant.

C

```
#include <stdatomic.h>

typedef enum {
    JOB_STATE_A_FAIRE  = 0x00, // En attente d'acquisition
    JOB_STATE_EN_COURS = 0x01, // Verrouillé par un thread de calcul
    JOB_STATE_TERMINE  = 0x02  // Traité, prêt pour le Worker 2 (RLE)
} NanoJobState;

typedef struct __attribute__((aligned(64))) {
    uint32_t nano_job_id;
    uint32_t context_id;
    
    // Coordonnées indexées du nano-chunk dans le Micro-Chunk [0..15]
    uint8_t local_nx;
    uint8_t local_ny;
    uint8_t local_nz;
    
    // Contrôle et supervision transactionnelle (Surveillant)
    _Atomic uint8_t job_state;  // État du job (CAS atomique par les workers)
    uint8_t worker_id;          // ID du thread de calcul assigné
    uint8_t padding;            // Alignement interne
    
    // Données géométriques injectées par le Worker 1 (AABB Intersections)
    uint16_t structure_count;   // Nombre d'objets traversant ce sous-volume
    uint32_t structure_ids[6];  // Identifiants des patterns à rasteriser (ex: Maison)
} NanoJob;
```

### B. Le Contexte de Production : `MicroChunkContext`

Cette structure est allouée dans une arène fixe par le Worker 0 au début de la requête d'extraction, puis complétée par les Workers de calcul.

C

```
#define NANO_PER_MICRO 4096 // 16x16x16 nano-chunks dans un micro-chunk

typedef struct __attribute__((aligned(64))) {
    uint32_t context_id;
    MacroChunkID macro_id;      // Clé primaire unifiée
    uint8_t lod_level;          // 0 (Précis), 1 (Moyen), 2 (Lointain)
    uint8_t is_allocated;       // Flag de cycle de vie dans l'arène fixe
    uint8_t padding[2];
    
    MacroWorldBlob world_meta;  // Données SQLite Main
    MacroRiverBlob river_meta;  // Données SQLite Rivers
    
    // Liste des deltas statiques injectés par le Worker 0
    uint32_t delta_count;
    uint32_t delta_positions[512]; // Coordonnées repackées des blocs modifiés
    uint8_t  delta_matieres[512];  // Nouvelles matières associées
    
    // La table de pointeurs intermédiaire (Lock-Free)
    // Remplie par les Workers de calcul, lue par le Worker 2 (RLE)
    uint8_t* nano_chunks_ptrs[NANO_PER_MICRO]; 
} MicroChunkContext;
```

## 4. Règles d'Arithmétique Binaire & Indexation Vectorielle

Pour maximiser les performances de calcul en temps réel à l'intérieur du pool de threads, toutes les indexations 3D spatiales ($16 \times 16 \times 16$) doivent utiliser des opérations de décalage de bits (`<<`, `>>`) et des masques binaires au lieu de multiplications ou divisions coûteuses.

### A. Indexation d'un pointeur dans le contexte (Worker 1 & Pool de Calcul)

L'index à une dimension dans le tableau `nano_chunks_ptrs` pour enregistrer un Nano-Chunk complété se calcule ainsi :

$$\text{index} = n_x + (n_y \ll 4) + (n_z \ll 8)$$

### B. Indexation d'un voxel à l'intérieur d'un Nano-Chunk de 4 Ko

Pour lire ou écrire un octet de matière à l'intérieur du buffer de 4 Ko ($16^3$) alloué par le pool de blocs libres :

$$\text{voxel\_index} = x + (y \ll 4) + (z \ll 8)$$

### C. Limites des coordonnées locales transmises par la Brique 3

La Brique 3 (Réseau) est responsable de la conversion des coordonnées absolues du joueur en indexations topologiques strictes :

- `mc_x`, `mc_z` $\in [0..19]$ (Car exactement 20 Micro-Chunks de $25.6\text{ m}$ composent un Macro-Chunk de $512\text{ m}$).
    
- `mc_y` $\in [0..119]$ (Car 120 Micro-Chunks de $25.6\text{ m}$ composent l'axe vertical absolu de $3072\text{ m}$).