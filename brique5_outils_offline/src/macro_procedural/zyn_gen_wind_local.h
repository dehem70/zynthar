#ifndef ZYN_GEN_WIND_LOCAL_H
#define ZYN_GEN_WIND_LOCAL_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_gen_wind_local.h
 * Date    : 31/05/2026
 * ============================================================================= */
#include <stdint.h>
#include <zynthar.h>
#include "zyn_gen_wind_global.h"

/**
 * @brief Calcule le vecteur de vent local altéré par la topographie d'un macro-chunk.
 * Fonction pure de complexité arithmétique constante O(1), sans allocation mémoire.
 *
 * @param map Pointeur constant vers la grille de MacroChunks.
 * @param width_x Largeur transversale de la carte (Axe X).
 * @param depth_z Longueur longitudinale de la carte (Axe Z).
 * @param chunk_x Coordonnée X du macro-chunk ciblé.
 * @param chunk_z Coordonnée Z du macro-chunk ciblé.
 * @return WindVector Le vecteur de vitesse locale (dx, dy) en km/h.
 */
WindVector zyn_gen_map_wind_local(const MacroChunk* map, int32_t width_x, int32_t depth_z, int32_t chunk_x, int32_t chunk_z);
#endif // ZYN_GEN_WIND_LOCAL_H
