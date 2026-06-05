#ifndef ZYN_GEN_MAP_HUMIDITY_H
#define ZYN_GEN_MAP_HUMIDITY_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_gen_map_humidity.h
 * Date    : 31/05/2026
 * Interface du générateur d'humidité macro-atmosphérique (Précipitations/Pluie).
 * ============================================================================= */

#include <stdint.h>
#include <zynthar.h>
#include "zyn_test_framework.h"

/**
 * @brief Calcule la carte d'humidité atmosphérique globale (pluie).
 * Version fusionnée, branchless et optimisée pour maximiser le débit du cache L1/L2.
 * Stocke temporairement le résultat brut [0-255] dans le champ 'biome' de chaque MacroChunk.
 *
 * @param map Pointeur vers la grille de MacroChunks à modifier.
 * @param width_x Largeur transversale de la carte (Axe X).
 * @param depth_z Longueur longitudinale de la carte (Axe Z).
 */
void zyn_gen_map_humidity(MacroChunk* map, int32_t width_x, int32_t depth_z,ZynTestConfig* test_config);
#endif // ZYN_GEN_MAP_HUMIDITY_H
