#ifndef ZYNTHAR_H
#define ZYNTHAR_H

#include <stdint.h>
#include <math.h>

// ============================================================================
// [SECTION 0] MACROS UTILITAIRES & LIENS EXTERNES
// ============================================================================

/**
 * @brief Calcul d'index linéaire pour matrices 2D aplaties.
 * Sécurisé contre les débordements par un cast systématique en size_t.
 */
#ifndef ZYN_INDEX
#define ZYN_INDEX(x, z, width) (((size_t)(z) * (size_t)(width)) + (size_t)(x))
#endif

/**
 * @brief Macro de journalisation unifiée du moteur.
 * Permet d'encapsuler ou de rediriger les flux de debug sans surcoût CPU.
 */
#define PRINT_BOTH(fmt, ...) do { printf(fmt, ##__VA_ARGS__); } while(0)

/**
 * @brief Binding natif vers la bibliothèque d'export STB Image.
 * Utilisé offline (B5) ou pour la maintenance (B7) afin de cartographier le monde.
 */
int stbi_write_png(char const *filename, int w, int h, int comp, void const *data, int stride_in_bytes);


// ============================================================================
// [SECTION 1] CONFIGURATION SYSTÈME, SUBSYSTÈMES & PERSISTANCE (BRIQUE 2 & 7)
// ============================================================================

#define ZYN_RAMDISK_PATH                "/dev/shm/zynthar_ramdisk"
#define ZYN_RAMDISK_SIZE_BYTES          (100 * 1024 * 1024) // Allocation stricte de 100 Mo
#define ZYN_CONFIG_PATH_ENV             "ZYNTHAR_ROOT"      // Variable d'environnement maître
#define ZYN_DEFAULT_BACKUP_INTERVAL_SEC 300                 // Cadence nominale du flush (5 minutes)

// --- Topologie du stockage et des bases SQLite3 ---
#define ZYN_DB_EMPLACEMENT              "data/"
#define ZYN_DB_WORLD                    "zyn-world.db"      // Squelette macro-procédural du relief
#define ZYN_DB_RIVER                    "zyn-river.db"      // Réseau hydrographique vectoriel
#define ZYN_DB_RESSOURCE                "zyn-ressource.db"  // Distribution statique des filons / entités
#define ZYN_DB_PLAYER                   "zyn-player.db"     // États persistants et wallets des joueurs
#define ZYN_DB_DELTA                    "zyn-delta.db"      // Modifications du monde par les utilisateurs

#define ZYN_CHRONOS_PORT                6969
#define ZYN_CHRONOS_ADDRESS             "127.0.0.1"
// ============================================================================
// [SECTION 2] PARAMÈTRES GÉOMÉTRIQUES & ÉCHELLES DE L'UNIVERS (ALIGNEMENT VOXEL)
// ============================================================================

// --- Dimensions de la Grille Terrestre ---
#define ZYN_WORLD_REGION_X      8     // Configuration de la carte : 8 régions en X
#define ZYN_WORLD_REGION_Z      4     // Configuration de la carte : 4 régions en Z
#define ZYN_WORLD_MACRO_DIM     256   // Résolution : 256 MacroChunks par côté de région

// Largeurs et profondeurs globales exprimées en nombre de MacroChunks
#define ZYN_WORLD_MACRO_WIDTH_X (ZYN_WORLD_REGION_X * ZYN_WORLD_MACRO_DIM)
#define ZYN_WORLD_MACRO_DEPTH_Z (ZYN_WORLD_REGION_Z * ZYN_WORLD_MACRO_DIM)

#define ZYN_TOTAL_MACRO_CHUNKS  (ZYN_WORLD_MACRO_WIDTH_X * ZYN_WORLD_MACRO_DEPTH_Z) // ~2 millions
#define ZYN_BAND_SIZE           50    // Zone tampon pour les calculs de raccordement aux frontières

// --- Bornes Altimétriques et Hydrographiques ---
#define ZYN_WORLD_Y_MIN         -1024 // Abysse max (Exactement 40 Micro-Chunks de 25.6m sous Y=0)
#define ZYN_WORLD_Y_MAX         2048  // Zénith max (Exactement 80 Micro-Chunks de 25.6m au-dessus de Y=0)
#define ZYN_SEA_LEVEL           0     // Point d'ancrage hydrographique (Coïncide avec une frontière de chunk)

// --- Métriques des structures de Voxels (Puissances de 2) ---
#define ZYN_VOXEL_TO_M          0.1f  // Résolution spatiale élémentaire : 1 voxel = 10 cm
#define ZYN_MICRO_CHUNK_DIM_VOX 256   // Résolution max d'un Micro-Chunk : 256^3 voxels
#define ZYN_MICRO_CHUNK_SHIFT   8     // Décalage de bits associé (2^8 = 256) pour divisions/multiplications CPU
#define ZYN_MACRO_CHUNK_DIM_M   512   // Longueur d'un Macro-Chunk : 512 mètres (Exactement 20 Micro-Chunks)
#define ZYN_NANO_CHUNK_DIM_VOX  16    // Unité de travail du cache CPU : 16^3 voxels (Page de 4 Ko)

// Limites physiques absolues du monde exprimées en mètres
#define ZYN_WORLD_X_MAX         (ZYN_WORLD_MACRO_WIDTH_X * ZYN_MACRO_CHUNK_DIM_M) // ~1 000 km
#define ZYN_WORLD_Z_MAX         (ZYN_WORLD_DEPTH_Z * ZYN_MACRO_CHUNK_DIM_M) // ~500 km


// ============================================================================
// [SECTION 3] PHYSIQUE, DÉPLACEMENT & COHÉRENCE GAMEPLAY (BRIQUE 1 & 4)
// ============================================================================

/**
 * @brief Seuils de franchissement verticaux (Exprimés en nombre strict de voxels de 10cm).
 * Évitent les branches conditionnelles complexes dans le moteur de collision AABB.
 */
#define ZYN_SEUIL_MARCHE_AUTO   4     // <= 40 cm : Enjambement fluide sans saut (ex: trottoirs)
#define ZYN_SEUIL_SAUT          10    // 40 cm à 1 m : Obstacle bas franchissable par impulsion
#define ZYN_SEUIL_ESCALADE      16    // 1 m à 1.6 m : Obstacle haut nécessitant traction des bras


// ============================================================================
// [SECTION 4] ID DES BIOMES & CONVERTISSEURS CLIMATIQUES
// ============================================================================

// --- Bornes Climatiques Réelles ---
#define ZYN_WORLD_TEMP_MIN      -25   // Température minimale en degrés Celsius
#define ZYN_WORLD_TEMP_MAX      45    // Température maximale en degrés Celsius

// --- Registre des Biomes Macroscopiques (uint8_t) ---
#define ZYN_BIOME_INCONNU           0
#define ZYN_BIOME_EAU_PROFONDE      1
#define ZYN_BIOME_EAU_COTIERE       2
#define ZYN_BIOME_PLAINE            3
#define ZYN_BIOME_DESERT            4
#define ZYN_BIOME_FORET             5
#define ZYN_BIOME_TAIGA             6
#define ZYN_BIOME_TOUNDRA           7
#define ZYN_BIOME_JUNGLE            8
#define ZYN_BIOME_GLACIER           9
#define ZYN_BIOME_PLAGE             10
#define ZYN_BIOME_MONTAGNE_ROCHEUSE 11
#define ZYN_BIOME_PIC_ENNEIGE       12
#define ZYN_BIOME_ABYSSE            13
#define ZYN_BIOME_EAU_INTERIEURE    14

// --- Outils d'Arithmétique Fixe (Mètres <-> Décimètres) ---
#define M_TO_DM(m)   ((int16_t)((m) * 10.0f))
#define DM_TO_M(dm)  (((float)(dm)) / 10.0f)

// --- Outils de Normalisation de Données (Celsius <-> uint8_t quantisé) ---
#define FLOAT_TO_RAW(f) ((uint8_t)(roundf(((f) - (ZYN_WORLD_TEMP_MIN)) / ((ZYN_WORLD_TEMP_MAX) - (ZYN_WORLD_TEMP_MIN)) * 255.0f)))
#define RAW_TO_FLOAT(r) ((float)(ZYN_WORLD_TEMP_MIN) + (((float)(r) / 255.0f) * ((float)(ZYN_WORLD_TEMP_MAX) - (ZYN_WORLD_TEMP_MIN))))


// ============================================================================
// [SECTION 5] TYPES DE DONNÉES & STRUCTURES MÉMOIRE MAÎTRESSE
// ============================================================================

/**
 * @brief Structure ultra-optimisée représentant un MacroChunk.
 * Poids total : Égal à 8 octets.
 * Alignement machine parfait (4 octets), aucun padding compilateur (zéro gâchis de RAM).
 */
typedef struct {
    uint8_t region_x;         /* 1 octet  | Coordonnée majeure de région X */ 
    uint8_t region_z;         /* 1 octet  | Coordonnée majeure de région Z */
    uint8_t chunk_x;          /* 1 octet  | Coordonnée macro relative X (0 à 255) */
    uint8_t chunk_z;          /* 1 octet  | Coordonnée macro relative Z (0 à 255) */
    int16_t elevation_max_dm; /* 2 octets | Altitude max en décimètres (-10240 à +20480) */
    uint8_t temperature_raw;  /* 1 octet  | Température quantisée normalisée (0 à 255) */
    uint8_t biome;            /* 1 octet  | ID unique du biome de surface (ZYN_BIOME_*) */
} MacroChunk;

// Ta structure Node pour la base de données vectorielle dédiée
typedef struct {
    uint8_t region_x;        /* 1 octets | Coordonnée region X */ 
    uint8_t region_z;        /* 1 octets | Coordonnée region X */
    uint8_t macro_x;         /* 1 octets | Coordonnée macro X */
    uint8_t macro_z;         /* 1 octets | Coordonnée macro Z */
    uint32_t data;           /* 4 octets :  Bits 17 à 20 (4 bits) : La direction (0 à 8)
                                            Bits 0 à 16 (17 bits) : Le flow_volume (0 à 120 000)
                                            Bits 21 à 31 (11 bits) : Libres / Inutilisés (mis à 0) */
} ZynRiverNode;

/**
 * @brief Clé composite pour requêtage direct ou indexation par table de hachage.
 * L'union permet un accès par structure sémantique ou par ID brut uint32_t sans coût de décalage de bits.
 */
typedef union {
    struct {
        int16_t x;
        int16_t z;
    } coord;
    uint32_t id;
} MacroKey;

/**
 * @brief Configuration de staging extraite du fichier de configuration global.
 */
typedef struct {
    char root_path[512];
    uint32_t backup_interval_seconds;
} CerbereConfig;

/**
 * @brief Arguments d'encapsulation de contexte pour le Worker asynchrone de Cerbère.
 * Aligné pour limiter la contamination des lignes de cache.
 */
typedef struct {
    char src_deltas[512];
    char dst_deltas[512];
    uint32_t interval_sec;
    volatile int keep_running;
} WatchdogArgs;

#endif // ZYNTHAR_H
