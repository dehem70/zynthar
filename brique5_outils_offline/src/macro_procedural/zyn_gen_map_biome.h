#ifndef ZYN_GEN_MAP_BIOME_H
#define ZYN_GEN_MAP_BIOME_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_gen_map_biome.h
 * Date    : 31/05/2026
 * Interface du sélecteur de biomes macroscopiques mondiaux.
 * ============================================================================= */

#include <stdint.h>
#include <zynthar.h>
#include "zyn_test_framework.h"


/**
 * @brief Calcule et attribue le biome final de chaque MacroChunk.
 * Consomme la température brute et l'humidité (stockée temporairement dans chunk->biome),
 * applique les surcharges bathymétriques, alpines et lacustres, puis écrit
 * l'ID de biome définitif dans chunk->biome.
 *
 * @param map Pointeur vers la grille de MacroChunks.
 * @param width_x Largeur de la carte.
 * @param depth_z Longueur de la carte.
 */
void zyn_gen_map_biome(MacroChunk* map, int32_t width_x, int32_t depth_z,ZynTestConfig* test_config);

#endif // ZYN_GEN_MAP_BIOME_H
