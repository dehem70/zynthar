#ifndef ZYN_GEN_MAP_TEMPERATURE_H
#define ZYN_GEN_MAP_TEMPERATURE_H

#include <stdint.h>

// Inclusion globale gérée par CMake
#include <zynthar.h>

/**
 * @brief Génère la carte de température macro en appliquant un gradient planétaire
 * symétrique (sandwich) perturbé par un bruit fractal et corrigé par l'altitude.
 *
 * Logique du gradient longitudinal :
 * - z = 0 : Froid polaire (0.0f)
 * - z = depth_z / 2 : Chaleur équatoriale (1.0f)
 * - z = depth_z : Froid polaire (0.0f)
 *
 * @param map Pointeur vers la grille de MacroChunks à modifier.
 * @param width_x Largeur transversale de la carte (Axe X).
 * @param depth_z Longueur longitudinale de la carte (Axe Z).
 */
void zyn_gen_map_temperature(MacroChunk* map, int32_t width_x, int32_t depth_z);

#endif /* ZYN_GEN_MAP_TEMPERATURE_H */
