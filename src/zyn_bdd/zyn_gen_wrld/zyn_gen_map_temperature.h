#ifndef ZYN_GEN_MAP_TEMPERATURE_H
#define ZYN_GEN_MAP_TEMPERATURE_H

#include "../../include/zynthar.h"

/**
 * @brief Génère la carte de température macro en appliquant un gradient planétaire
 * symétrique (sandwich) perturbé par un bruit fractal.
 * * Logique du gradient :
 * - y = 0 : Froid polaire (0.0f)
 * - y = height / 2 : Chaleur équatoriale (1.0f)
 * - y = height : Froid polaire (0.0f)
 * * @param map Pointeur vers la grille de MacroChunks à modifier.
 * @param width Largeur de la carte.
 * @param height Hauteur de la carte.
 */
void zyn_gen_map_temperature(MacroChunk* map, int32_t width, int32_t height);

#endif /* ZYN_GEN_MAP_TEMPERATURE_H */
