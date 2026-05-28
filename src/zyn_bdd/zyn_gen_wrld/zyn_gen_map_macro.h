#ifndef ZYN_GEN_MAP_MACRO_H
#define ZYN_GEN_MAP_MACRO_H

#include "../../include/zynthar.h"
#include <stdbool.h>

/* =============================================================================
 * INTERFACE DU GÉNÉRATEUR GÉOMORPHOLOGIQUE MACRO
 * ============================================================================= */

/**
 * @brief Alloue dynamiquement en mémoire une grille plate de MacroChunks.
 * @param width Largeur de la carte (X).
 * @param height Hauteur/Longueur de la carte (Y).
 * @return Pointeur vers le tableau de MacroChunks alloué, ou NULL en cas d'échec.
 */
MacroChunk* zyn_gen_map_macro_alloc(int32_t width, int32_t height);

/**
 * @brief Libère la mémoire occupée par la grille de MacroChunks.
 * @param map Pointeur vers la grille à libérer.
 */
void zyn_gen_map_macro_free(MacroChunk* map);

/**
 * @brief Génère le masque de Voronoi/Worley pour délimiter les centres de masses terrestres.
 * @param map Grille de MacroChunks à modifier.
 * @param width Largeur de la carte.
 * @param height Hauteur de la carte.
 * @param num_islands Nombre d'îles à positionner (NB_ILES).
 */
void zyn_gen_map_macro_voronoi(MacroChunk* map, int32_t width, int32_t height, int32_t num_islands);

/**
 * @brief Combine le relief fractal et le masque de Voronoi pour sculpter l'archipel
 * et calibre l'altitude pour respecter le pourcentage de mer maximum.
 * @param map Grille de MacroChunks à modifier.
 * @param width Largeur de la carte.
 * @param height Hauteur de la carte.
 * @param num_islands Nombre d'îles.
 * @param max_sea_percentage Proportion cible de l'eau (ex: 0.45f pour 45%).
 */
void zyn_gen_map_macro_archipelago(MacroChunk* map, int32_t width, int32_t height, int32_t num_islands, float max_sea_percentage);

/**
 * @brief Applique un automate cellulaire (Voisinage de Moore) pour nettoyer et
 * lisser les lignes de côtes côtières de l'archipel.
 * @param map Grille de MacroChunks à modifier.
 * @param width Largeur de la carte.
 * @param height Hauteur de la carte.
 * @param iterations Nombre de passes de lissage (ex: 3).
 */
void zyn_gen_map_macro_smooth_coastlines(MacroChunk* map, int32_t width, int32_t height, int32_t iterations);

#endif /* ZYN_GEN_MAP_MACRO_H */
