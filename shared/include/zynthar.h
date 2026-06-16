#ifndef ZYNTHAR_H
#define ZYNTHAR_H

#include <stdint.h>
#include <math.h>

// ============================================================================
// [SECTION 1] CONFIGURATION SYSTÈME, SUBSYSTÈMES & PERSISTANCE (BRIQUE 2 & 7)
// ============================================================================

#define ZYN_RAMDISK_PATH                "/dev/shm/zynthar_ramdisk"
#define ZYN_CONFIG_PATH_ENV             "ZYNTHAR_ROOT"      // Variable d'environnement maître

// --- Topologie du stockage et des bases SQLite3 ---
#define ZYN_DB_EMPLACEMENT              "data/"
#define ZYN_DB_WORLD                    "zyn-world.db"      // Squelette macro-procédural du relief
#define ZYN_DB_RIVER                    "zyn-river.db"      // Réseau hydrographique vectoriel
#define ZYN_DB_RESSOURCE                "zyn-ressource.db"  // Distribution statique des filons / entités
#define ZYN_DB_PLAYER                   "zyn-player.db"     // États persistants et wallets des joueurs
#define ZYN_DB_DELTA                    "zyn-delta.db"      // Modifications du monde par les utilisateurs

// ============================================================================
// [SECTION 2] VARIABLES DE DESCRIPTION DU MONDE
// ============================================================================

/* -----------------------------------------------------------------------------
 * 1. CONFIGURATION DE L'ATOME (VOXEL)
 * -------------------------------------------------------------------------- */
#define ZYN_VOXEL_SIZE_M          0.1f                      /* Taille d'un voxel : 10 cm (0.1 mètre) */
#define ZYN_VOXELS_PER_METER      (1/ZYN_VOXELS_PER_METER)  /* Nombre de voxels dans un mètre linéaire */


/* -----------------------------------------------------------------------------
 * 2. CONFIGURATION GÉOGRAPHIQUE DU MONDE (LOD 3)
 * -------------------------------------------------------------------------- */
#define ZYN_WORLD_REGION_X        256       /* 256 régions en X */
#define ZYN_WORLD_REGION_Z        128       /* 128 régions en Z */
#define ZYN_TOTAL_REGIONS         (ZYN_WORLD_REGION_X * ZYN_WORLD_REGION_Z)

#define ZYN_WORLD_MACRO_DIM       64        /* Une région = 64x64 Macro-Chunks */

/* Nombre total de Macro-Chunks dans la base de données (134 217 228) */
#define ZYN_TOTAL_MACRO_CHUNKS    (ZYN_TOTAL_REGIONS * ZYN_WORLD_MACRO_DIM * ZYN_WORLD_MACRO_DIM)

#define ZYN_BAND_SIZE           50    // Zone tampon pour les calculs de raccordement aux frontières

/* -----------------------------------------------------------------------------
 * 3. COUPE VERTICALE DU MONDE (ALTITUDES)
 * -------------------------------------------------------------------------- */
#define ZYN_WORLD_HEIGHT_MIN_M    -1024     /* Altitude minimale en mètres */
#define ZYN_WORLD_HEIGHT_MAX_M    2048      /* Altitude maximale en mètres */
#define ZYN_WORLD_HEIGHT_TOTAL_M  3072      /* Hauteur utile totale (3 km) */
#define ZYN_SEA_LEVEL             0         // Point d'ancrage hydrographique (Coïncide avec une frontière de chunk)


/* -----------------------------------------------------------------------------
 * 4. GRANULOMÉTRIE DES BLOCS ET RECOUVREMENT (LOD 3 -> LOD 2 -> LOD 1)
 * -------------------------------------------------------------------------- */

/* LOD 3 : Macro-Chunk (Unité d'autorité DB) */
#define ZYN_MACRO_CHUNK_DIM_M     64        /* 64 mètres de côté */

/* LOD 2 : Meso-Chunk (Unité d'échange I/O) */
#define ZYN_MESO_CHUNK_DIM_M      16        /* 16 mètres de côté */
#define ZYN_MACRO_TO_MESO_RATIO   4         /* Un Macro-Chunk contient 4x4 Meso-Chunks */

/* LOD 1 : Micro-Chunk (Unité de calcul RAW interne) */
#define ZYN_MICRO_CHUNK_DIM_M     4         /* 4 mètres de côté */

#define ZYN_MESO_TO_MICRO_RATIO   ( ZYN_MESO_CHUNK_DIM_M /  ZYN_MICRO_CHUNK_DIM_M)

/* -----------------------------------------------------------------------------
 * 5. DIMENSIONS EN VOXELS ET VOLUMÉTRIE MÉMOIRE (LOD 1 INTERNE)
 * -------------------------------------------------------------------------- */
#define ZYN_MICRO_CHUNK_VOXEL_DIM     (ZYN_MICRO_CHUNK_DIM_M * ZYN_VOXELS_PER_METER)

#define ZYN_MESO_CHUNK_TOTAL_MICRO   (ZYN_MESO_TO_MICRO_RATIO *ZYN_MESO_TO_MICRO_RATIO *ZYN_MESO_TO_MICRO_RATIO ) 

/* Nombre total de voxels dans un Micro-Chunk de travail (40^3 = 64 000 voxels) */
#define ZYN_MICRO_CHUNK_TOTAL_VOXELS (ZYN_MICRO_CHUNK_VOXEL_DIM * ZYN_MICRO_CHUNK_VOXEL_DIM * ZYN_MICRO_CHUNK_VOXEL_DIM)

/* Poids en mémoire vive (RAM / Cache L2) d'un Micro-Chunk en mode RAW (64 Ko) */
#define ZYN_MICRO_CHUNK_RAW_SIZE_BYTES ZYN_MICRO_CHUNK_TOTAL_VOXELS 

/* Poids en mémoire vive d'un Micro-Chunk compressé en Palette 4 bits (~32 Ko) */
#define ZYN_MICRO_CHUNK_PALETTE_SIZE_BYTES (16 + (ZYN_MICRO_CHUNK_TOTAL_VOXELS / 2))


/* -----------------------------------------------------------------------------
 * 6. RECOUVREMENT VERTICAL (SÉCURITÉ INDUSTRIELLE)
 * -------------------------------------------------------------------------- */
/* Nombre de Micro-Chunks nécessaires pour couvrir toute la hauteur du monde (3072m / 4m = 768) */
#define ZYN_TOTAL_VERTICAL_MICRO_CHUNKS (ZYN_WORLD_HEIGHT_TOTAL_M / ZYN_MICRO_CHUNK_DIM_M)


/* -----------------------------------------------------------------------------
 * 7. LIMITATION DES MATIÈRES / CONFIGURATION DU SYSTEME
 * -------------------------------------------------------------------------- */
#define ZYN_MAX_GLOBAL_MATERIALS  128       /* ID max codé sur le uint8_t en mode RAW */
#define ZYN_MAX_PALETTE_MATERIALS 16        /* ID max par palette locale (Codage 4 bits) */



// ============================================================================
// [SECTION 0] MACROS UTILITAIRES & LIENS EXTERNES
// ============================================================================

/**
 * @brief Calcul d'index linéaire pour matrices 2D aplaties.
 * Sécurisé contre les débordements par un cast systématique en size_t.
 */
#ifndef ZYN_INDEX
#define ZYN_INDEX(x, z, y) (((size_t)(z) * (size_t)(y)) + (size_t)(x))
#endif

/**
 * @brief Macro de journalisation unifiée du moteur.
 * Permet d'encapsuler ou de rediriger les flux de debug sans surcoût CPU.
 */
#define PRINT_BOTH(fmt, ...) do { printf(fmt, ##__VA_ARGS__); } while(0)





#define HECATE_MAT_AIR      0
#define HECATE_MAT_ROCHE    1
#define HECATE_MAT_EAU      2
#define HECATE_MAT_SABLE    3  
#define HECATE_MAT_TERRE    4  
#define HECATE_MAT_MIXTE    255        // Le matériau d'un Micro-Chunk hétérogène (8 bits)

// Dans ton fichier d'en-tête commun (ex: zyn_hecate_utils.h ou zynthar.h)
#define HECATE_STATE_AIR    0xFFFFFFF0
#define HECATE_STATE_ROCHE  0xFFFFFFF1
#define HECATE_STATE_EAU    0xFFFFFFF2
#define HECATE_STATE_MIXTE  0xFFFFFFFF // Ton 255 d'origine entre en collision avec l'index de pool 255 !

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
#define ZYN_SEUIL_SAUT          12    // 40 cm à 1.2 m : Obstacle bas franchissable par impulsion
#define ZYN_SEUIL_ESCALADE      20    // 1 m à 2 m : Obstacle haut nécessitant traction des bras


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

// ============================================================================
// CATALOGUE DES MATIÈRES - Spécification uint8_t
// ============================================================================

// --- 0x00 à 0x0F : Fluides et Vides ---
#define ZYN_MAT_AIR                 0   
#define ZYN_MAT_EAU                 1   
#define ZYN_MAT_EAU_GELANTE         2   

// --- 0x10 à 0x2F : Sols Mobiles et Sédiments ---
#define ZYN_MAT_TERRE               16  
#define ZYN_MAT_HERBE               17  
#define ZYN_MAT_SABLE               18  
#define ZYN_MAT_SABLE_MOUILLE       19  
#define ZYN_MAT_GRAVIER             20  
#define ZYN_MAT_BOUE               21  
#define ZYN_MAT_ARGILE              22  

// --- 0x30 à 0x4F : Couvertures Thermiques et Cryosphère ---
#define ZYN_MAT_NEIGE_FRAICHE       48  
#define ZYN_MAT_NEIGE_COMPACTE     49  
#define ZYN_MAT_GLACE               50  
#define ZYN_MAT_GLACE_BLEUE         51  
#define ZYN_MAT_PERGELISOL          52  

// --- 0x50 à 0x6F : Roches et Couches Profondes ---
#define ZYN_MAT_ROCHE_FRAGILE       80  
#define ZYN_MAT_ROCHE_FRANCHE       81  
#define ZYN_MAT_ROCHE_SOMBRE        82  

// --- 0x70 à 0x8F : Matières Végétales Composantes ---
#define ZYN_MAT_MOUSSE              112 
#define ZYN_MAT_HUMUS_JUNGLE        113 

// ============================================================================
// --- 0x90 à 0xAF : RESSOURCES NATURELLES EXTRACIBLES
// ============================================================================
#define ZYN_MAT_BOIS_TRONC          144 // Tronc d'arbre standard (Forêt/Taïga)
#define ZYN_MAT_BOIS_JUNGLE         145 // Tronc d'arbre tropical (Jungle)
#define ZYN_MAT_FEUILLAGE           146 // Voxel de feuilles (généré par le décorateur)
#define ZYN_MAT_FILON_CHARBON       150 // Ressource basique (énergie/fusion)
#define ZYN_MAT_FILON_CUIVRE        151 // Minerai technologique de transition
#define ZYN_MAT_FILON_FER           152 // Minerai structurel majeur
#define ZYN_MAT_FILON_OR            153 // Minerai précieux
#define ZYN_MAT_FILON_ZYNTHARITE    155 // Minerai rare mythique (Lien direct Web3 / Économie)

// ============================================================================
// --- 0xB0 à 0xDF : MATÉRIAUX DE CONSTRUCTION (Posés par les joueurs)
// ============================================================================
#define ZYN_MAT_STRUCT_BOIS_PLANCHES 176 // Blocs de construction en bois raffiné
#define ZYN_MAT_STRUCT_PIERRE_TAILLE 177 // Blocs de maçonnerie (murs, citadelles)
#define ZYN_MAT_STRUCT_BRIQUE_ARGILE 178 // Brique cuite (esthétique/résistance moyenne)
#define ZYN_MAT_STRUCT_BETON_BRUT    179 // Matériau industriel lourd
#define ZYN_MAT_STRUCT_VERRE_PAVÉ    180 // Premier bloc transparent (transmet la lumière)
#define ZYN_MAT_STRUCT_METAL_PLAQUE  181 // Blindage haut niveau


#define ZYN_MAT_MIXTE                255 // Le matériau d'un bloc hétérogène


// --- Outils d'Arithmétique Fixe (Mètres <-> Décimètres) ---
#define M_TO_DM(m)   ((int16_t)((m) * 10.0f))
#define DM_TO_M(dm)  (((float)(dm)) / 10.0f)

// --- Outils de Normalisation de Données (Celsius <-> uint8_t quantisé) ---
#define FLOAT_TO_RAW(f) ((uint8_t)(roundf(((f) - (ZYN_WORLD_TEMP_MIN)) / ((ZYN_WORLD_TEMP_MAX) - (ZYN_WORLD_TEMP_MIN)) * 255.0f)))
#define RAW_TO_FLOAT(r) ((float)(ZYN_WORLD_TEMP_MIN) + (((float)(r) / 255.0f) * ((float)(ZYN_WORLD_TEMP_MAX) - (ZYN_WORLD_TEMP_MIN))))


// ============================================================================
// [SECTION 5] TYPES DE DONNÉES & STRUCTURES MÉMOIRE MAÎTRESSE
// ============================================================================
#define ZYN_STATUS_COMPUTING 0
#define ZYN_STATUS_READY 1
#define ZYN_STATUS_COMPRESSED 3
#define ZYN_STATUS_FREE        255

/**
 * @brief Structure ultra-optimisée représentant un MacroChunk.
 * Poids total : Égal à 8 octets.
 * Alignement machine parfait (4 octets), aucun padding compilateur (zéro gâchis de RAM).
 */
typedef struct {
    uint8_t region_x;          /* 1 octet  | Coordonnée majeure de région X */ 
    uint8_t region_z;          /* 1 octet  | Coordonnée majeure de région Z */
    uint8_t chunk_x;           /* 1 octet  | Coordonnée macro relative X (0 à 255) */
    uint8_t chunk_z;           /* 1 octet  | Coordonnée macro relative Z (0 à 255) */
    int16_t elevation_max_dm;  /* 2 octets | Altitude max en décimètres (-10240 à +20480) */
    int16_t elevation_coin_nw; /* 2 octets | Altitude max en décimètres (-10240 à +20480) */
    int16_t elevation_coin_ne; /* 2 octets | Altitude max en décimètres (-10240 à +20480) */
    int16_t elevation_coin_se; /* 2 octets | Altitude max en décimètres (-10240 à +20480) */
    int16_t elevation_coin_sw; /* 2 octets | Altitude max en décimètres (-10240 à +20480) */
    uint8_t temperature_raw;   /* 1 octet  | Température quantisée normalisée (0 à 255) */
    uint8_t biome;             /* 1 octet  | ID unique du biome de surface (ZYN_BIOME_*) */
} MacroChunk;

typedef struct {
    int16_t elevation_max_dm;  /* 2 octets | Altitude max globale */
    int16_t elevation_coin_nw; /* 2 octets | Altitude Nord-Ouest */
    int16_t elevation_coin_ne; /* 2 octets | Altitude Nord-Est */
    int16_t elevation_coin_se; /* 2 octets | Altitude Sud-Est */
    int16_t elevation_coin_sw; /* 2 octets | Altitude Sud-Ouest */
    uint8_t temperature_raw;   /* 1 octet  | Température */
    uint8_t biome;             /* 1 octet  | ID unique du biome */
} MacroChunk_db; // Taille totale en RAM : 12 octets

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
        uint8_t z;    // Octet de poids faible (Bits 0-7)
        uint8_t x;    // (Bits 8-15)
        uint8_t rz;   // (Bits 16-23)
        uint8_t rx;   // Octet de poids fort (Bits 24-31)
    };
    uint32_t id;     // Les 4 octets combinés en un seul entier
} Id;

#endif // ZYNTHAR_H
