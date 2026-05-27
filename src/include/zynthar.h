#ifndef ZYNTHAR_H
#define ZYNTHAR_H

// --- Configurations de la Base de Données ---
#define ZYN_DB_EMPLACEMENT     	"zyn_db/"
#define ZYN_DB_WORLD           	"zyn-world.db"
#define ZYN_DB_PLAYER          	"zyn-player.db"
#define ZYN_DB_DELTA           	"zyn-delta.db"

// --- Paramètres de l'Univers de Zynthar ---
#define ZYN_X_MAX              	1000000  // 1000 km
#define ZYN_Y_MAX              	500000   // 500 km
#define ZYN_Z_MIN		-1000    // 1 km de profondeur
#define ZYN_Z_MAX               2000     // 2 km de hauteur
#define ZYN_CHUNK_MACRO_DIM     500      // 500 m
#define ZYN_CHUNK_MICRO_DIM	25.6     // 25.6 m
#define ZYN_VOXEL_TO_M		0.1	 // 0.1 m de coté
#define ZYN_NIV_MER		0

// --- Identifiants des Biomes ---
#define BIOME_UNKNOWN          	0
#define BIOME_DESERT           	1
#define BIOME_PLAINS           	2
#define BIOME_FOREST           	3
#define BIOME_MOUNTAINS        	4
#define BIOME_TUNDRA           	5

#endif // ZYNTHAR_H
