#ifndef ZYNTHAR_H
#define ZYNTHAR_H

#include <stdint.h>
#include <math.h>

#define ZYN_INDEX(x, z, width) (((size_t)(z) * (size_t)(width)) + (size_t)(x))

// --- Configurations des bases de Données ---
#define ZYN_DB_EMPLACEMENT      "data/"
#define ZYN_DB_WORLD            "zyn-world.db"
#define ZYN_DB_PLAYER           "zyn-player.db"
#define ZYN_DB_DELTA            "zyn-delta.db"

// --- Paramètres de l'Univers de Zynthar (Alignement Voxel / Chunk Parfait) ---
#define ZYN_WORLD_X_MAX         1024256  // Longueur : 1000.96 km (39100 Micro-Chunks)
#define ZYN_WORLD_Z_MAX         512256   // Largeur : 500.224 km   (19540 Micro-Chunks)

#define ZYN_WORLD_Y_MIN         -1024    // Profondeur max : -1024 m (Exactement 40 Micro-Chunks sous Y=0)
#define ZYN_WORLD_Y_MAX         2048     // Hauteur max : +2048 m     (Exactement 80 Micro-Chunks au-dessus de Y=0)
#define ZYN_SEA_LEVEL           0        // Niveau de la mer = Frontière stricte de Chunk
#define ZYN_WORLD_TEMP_MIN      -25
#define ZYN_WORLD_TEMP_MAX      45

// --- Dimensions Structurelles Géométriques (Essentielles au moteur) ---
#define ZYN_VOXEL_TO_M          0.1f     // Un voxel = 10 cm de côté
#define ZYN_MICRO_CHUNK_DIM_VOX 256      // 256 voxels de côté (25.6m)
#define ZYN_MICRO_CHUNK_SHIFT   8        // 2^8 = 256 (Pour les décalages de bits rapides)
#define ZYN_MACRO_CHUNK_DIM_M   512      // 512m de côté (Exactement 20 Micro-Chunks)

// --- Seuils Physiques de Déplacement (Exprimés en nombre de voxels de 10cm) ---
#define ZYN_SEUIL_MARCHE_AUTO   4        // <= 40 cm (1 à 4 blocs)
#define ZYN_SEUIL_SAUT          10       // 40 cm à 1 m (5 à 10 blocs)
#define ZYN_SEUIL_ESCALADE      16       // 1 m à 1.6 m (11 à 16 blocs)

// --- Identifiants Uniques des Biomes Macroscopiques (uint8_t) ---
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

// Outils de conversion d'unités (Mètres <-> Décimètres)
#define M_TO_DM(m)   (int16_t)((m) * 10.0f)
#define DM_TO_M(dm)  (float)((dm) / 10.0f)

// Outils de conversion climatiques corrigés et sécurisés par parenthésage strict
#define FLOAT_TO_RAW(f)  ((uint8_t)(roundf(((f) - (ZYN_WORLD_TEMP_MIN)) / ((ZYN_WORLD_TEMP_MAX) - (ZYN_WORLD_TEMP_MIN)) * 255.0f)))
#define RAW_TO_FLOAT(r)  ((float)(ZYN_WORLD_TEMP_MIN) + (((float)(r) / 255.0f) * ((float)(ZYN_WORLD_TEMP_MAX) - (ZYN_WORLD_TEMP_MIN))))

/**
 * @brief Structure ultra-optimisée représentant un MacroChunk.
 * Taille totale : 12 octets.
 * Alignement parfait à 4 octets, zéro padding invisible.
 */
typedef struct {
    int32_t chunk_x;         /* 4 octets | Coordonnée macro X */
    int32_t chunk_z;         /* 4 octets | Coordonnée macro Z */
    int16_t elevation_max_dm;/* 2 octets | Altitude max en décimètres (-10240 à +20480) */
    uint8_t temperature_raw; /* 1 octet  | Température normalisée (0 à 255 -> 0% à 100%) */
    uint8_t biome;           /* 1 octet  | ID du biome (ZYN_BIOME_*) */
} MacroChunk;

#endif // ZYNTHAR_H
