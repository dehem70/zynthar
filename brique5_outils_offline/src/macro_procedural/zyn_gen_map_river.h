#ifndef ZYN_GEN_MAP_RIVER_H
#define ZYN_GEN_MAP_RIVER_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_gen_map_river.h
 * Date    : 31/05/2026
 * Interface du générateur hydrographique (Rivières et Lacs).
 * ============================================================================= */
#include <stdint.h>
#include <zynthar.h>

/*
 * @brief Structure représentant un nœud ou segment de rivière calculé à l'échelle Macro.
 * Conforme au futur format d'enregistrement en Base de Données SQLite.
 */
typedef struct {
    int32_t macro_x;       /* Coordonnée macro X */
    int32_t macro_z;       /* Coordonnée macro Z (Axe longitudinal) */
    uint8_t entry_micro_x; /* Point d'ancrage d'entrée local Micro X (0 à 19) */
    uint8_t entry_micro_z; /* Point d'ancrage d'entrée local Micro Z (0 à 19) */
    uint8_t direction;     /* Direction d'écoulement vers le voisin macro (1 à 8) */
    uint32_t flow_volume;  /* Débit ou volume accumulé */
} ZynRiverNode;

/**
 * @brief Calcule le réseau hydrographique directement sur la grille des MacroChunks.
 *
 * @param map Pointeur vers la grille de MacroChunks.
 * @param width_x Largeur transversale de la carte.
 * @param depth_z Longueur longitudinale de la carte.
 * @param out_macro_flux_grid Tableau de uint32_t de taille (width_x * depth_z) alloué
 * par l'appelant pour stocker le flux accumulé macro.
 */
void zyn_gen_map_river(MacroChunk* map, int32_t width_x, int32_t depth_z, uint32_t* out_macro_flux_grid);

/**
 * @brief Inonde une cuvette macro à partir d'un point de blocage.
 * Remplit les dépressions adjacentes et cherche le point de débordement (Spillpoint).
 * Marks les chunks inondés (lac) dans map->biome.
 * * @return size_t Index du Macro Chunk correspondant au col de débordement (exutoire).
 */
static size_t zyn_hydro_flood_cuvette(MacroChunk* map, int32_t width_x, int32_t depth_z, int32_t start_cx, int32_t start_cz, int32_t* out_exit_cx, int32_t* out_exit_cz);

#endif // ZYN_GEN_MAP_RIVER_H
