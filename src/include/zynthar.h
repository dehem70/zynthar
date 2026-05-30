#ifndef ZYNTHAR_H

#define ZYNTHAR_H



#include <stdint.h>



// --- Configurations de la Base de Données ---

#define ZYN_DB_EMPLACEMENT      "zyn_db/"

#define ZYN_DB_WORLD            "zyn-world.db"

#define ZYN_DB_PLAYER          "zyn-player.db"

#define ZYN_DB_DELTA            "zyn-delta.db"



// --- Paramètres de l'Univers de Zynthar ---

#define ZYN_X_MAX              1000000  // 1000 km

#define ZYN_Y_MAX              500000   // 500 km

#define ZYN_Z_MIN -1000    // 1 km de profondeur

#define ZYN_Z_MAX               2000     // 2 km de hauteur

#define ZYN_CHUNK_MACRO_DIM     500      // 500 m

#define ZYN_CHUNK_MICRO_DIM 25.6     // 25.6 m

#define ZYN_VOXEL_TO_M 0.1 // 0.1 m de coté

#define ZYN_NIV_MER 0



/* =============================================================================

 * ENUMERATIONS & STRUCTURES POUR LA GENERATION MACRO (MACRO_CHUNKS)

 * ============================================================================= */



/**

 * @brief Énumération des biomes macroscopiques.

 * Associe chaque type de environnement à un identifiant numérique unique.

 */

typedef enum {

    BIOME_INCONNU = 0,

    BIOME_EAU_PROFONDE = 1,

    BIOME_EAU_COTIERE = 2,

    BIOME_PLAINE = 3,

    BIOME_DESERT = 4,

    BIOME_FORET = 5,

    BIOME_TAIGA = 6,

    BIOME_TOUNDRA = 7,

    BIOME_JUNGLE = 8,

    BIOME_GLACIER = 9,

    BIOME_PLAGE = 10,

    BIOME_MONTAGNE_ROCHEUSE = 11,

    BIOME_PIC_ENNEIGE = 12

} MacroBiome;



/**

 * @brief Structure représentant la carte d'identité immuable d'un MacroChunk.

 * Alignée en mémoire pour faire face aux exigences de performance et de stockage SQLite3.

 */

typedef struct {

    int32_t x;               /* Coordonnée horizontale X */

    int32_t y;               /* Coordonnée horizontale Y (axe longitudinal) */

    float elevation_max;     /* Altitude maximale théorique (Z) */

    float temperature;       /* Facteur climatique : Température Macro */

    float humidity;          /* Facteur climatique : Humidité Macro */

    MacroBiome biome;        /* Biome calculé selon Whittaker modifié */

} MacroChunk;



#endif // ZYNTHAR_H 


