### 📑 RÉFÉRENTIEL TECHNIQUE D'I/O — BRIQUE 2 (PERSISTANCE)

#### 1. TOPOLOGIE DES IDENTIFIANTS UNIQUE (Clé Primaire)

L'identifiant unique d'un Macro-Chunk dans toutes les bases SQLite3 est un entier 32 bits sans signe (`uint32_t`) mappé sur une union C pour éliminer le coût des décalages de bits lors de l'accès aux coordonnées :

C

```
typedef union {
    struct {
        uint8_t rx; // Région X [0..255]
        uint8_t rz; // Région Z [0..255]
        uint8_t x;  // Macro-Chunk X local à la région [0..255]
        uint8_t z;  // Macro-Chunk Z local à la région [0..255]
    };
    uint32_t id;    // Clé primaire SQLite3 (macro_id)
} MacroId;
```

#### 2. FORMATS DES SCHÉMAS ET BLOBS EN BASE DE DONNÉES

Chaque base de données utilise le modificateur `WITHOUT ROWID` indexé sur la clé primaire `id` pour maximiser le débit des requêtes d'agrégation.

##### A. Base du Relief (`zyn_world.db`)

- **Schéma SQL :** `CREATE TABLE IF NOT EXISTS macro_chunks (id INTEGER PRIMARY KEY, data BLOB NOT NULL) WITHOUT ROWID ;`
    
- **Structure du BLOB (8 octets, sans padding) :**
    

C

```
#pragma pack(push, 1)
typedef struct {
    uint8_t region_x;
    uint8_t region_z;
    uint8_t macro_x;
    uint8_t macro_z;
    int16_t elevation_max_dm; // Altitude max en dm (-10240 à +20480)
    uint8_t temperature_raw;  // Climat normalisé [0..255]
    uint8_t biome;            // ID Métier du biome
} MacroChunk;
#pragma pack(pop)
```

##### B. Base des Rivières (`zyn_rivers.db`)

- **Schéma SQL :** `CREATE TABLE IF NOT EXISTS macro_chunks (id INTEGER PRIMARY KEY, data BLOB NOT NULL) WITHOUT ROWID ;`
    
- **Structure du BLOB (8 octets, sans padding) :**
    

C

```
#pragma pack(push, 1)
typedef struct {
    uint8_t region_x;
    uint8_t region_z;
    uint8_t macro_x;
    uint8_t macro_z;
    uint32_t data;            // Bits 0-16: Flow Volume | Bits 17-20: Direction
} ZynRiverNode;
#pragma pack(pop)
```

#### 3. PROTOCOLE D'ÉCHANGE INTER-BRIQUES (B3 -> B2)

La Brique 3 (Réseau) est responsable de la conversion des coordonnées absolues du joueur en indexations topologiques. Elle appelle la Brique 2 via l'interface stricte :

C

```
ActiveMicroChunk B2_GenerateMicroChunk(uint32_t macro_id, uint8_t mc_x, uint8_t mc_y, uint8_t mc_z, uint8_t lod_level);
```

- **Limites des coordonnées locales transmises :**
    
    - `mc_x`, `mc_z` $\in [0..19]$ (Car 20 Micro-Chunks de $25.6\text{ m}$ dans $512\text{ m}$).
        
    - `mc_y` $\in [0..119]$ (Car 120 Micro-Chunks de $25.6\text{ m}$ dans l'axe vertical de $3072\text{ m}$).