#ifndef ZYN_GEN_MAP_RELIEF_H
#define ZYN_GEN_MAP_RELIEF_H

#include <stdint.h>
#include <stdbool.h>

// Inclusion globale gérée par CMake
#include <zynthar.h>
#include "zyn_test_framework.h"
#include "zyn_utils.h"

/* =============================================================================
 * INTERFACE DU GÉNÉRATEUR GÉOMORPHOLOGIQUE MACRO
 * ============================================================================= */

/**
 * @brief Alloue dynamiquement en mémoire une grille plate de MacroChunks.
 * @param width_x Largeur transversale de la carte (Axe X).
 * @param depth_z Longueur longitudinale de la carte (Axe Z).
 * @return Pointeur vers le tableau de MacroChunks alloué, ou NULL en cas d'échec.
 */
MacroChunk* zyn_gen_map_relief_alloc(int32_t width_x, int32_t depth_z);

/**
 * @brief Libère la mémoire occupée par la grille de MacroChunks.
 * @param map Pointeur vers la grille à libérer.
 */
void zyn_gen_map_relief_free(MacroChunk* map);

/**
 * @brief Génère le masque de Voronoi pour délimiter les centres de masses terrestres.
 * @param width_x Largeur transversale de la carte (Axe X).
 * @param depth_z Longueur longitudinale de la carte (Axe Z).
 * @param num_islands Nombre d'îles à positionner.
 */
float* zyn_gen_map_relief_voronoi(int32_t width_x, int32_t depth_z, int32_t num_islands, uint32_t seed); 

/**
 * @brief Combine le relief fractal et le masque de Voronoi pour sculpter l'archipel
 * et calibre l'altitude pour respecter le pourcentage de mer maximum.
 * @param map Grille de MacroChunks à modifier.
 * @param width_x Largeur transversale de la carte (Axe X).
 * @param depth_z Longueur longitudinale de la carte (Axe Z).
 * @param num_islands Nombre d'îles.
 * @param max_sea_percentage Proportion cible de l'eau (ex: 0.45f pour 45%).
 */
void zyn_gen_map_relief_archipelago(MacroChunk* map, int32_t width_x, int32_t depth_z, int32_t num_islands, float max_sea_percentage, uint32_t seed,ZynTestConfig* test_config);

/**
 * @brief Applique un automate cellulaire (Voisinage de Moore) pour nettoyer et
 * lisser les lignes de côtes de l'archipel.
 * @param map Grille de MacroChunks à modifier.
 * @param width_x Largeur transversale de la carte (Axe X).
 * @param depth_z Longueur longitudinale de la carte (Axe Z).
 * @param iterations Nombre de passes de lissage (ex: 3).
 */
void zyn_gen_map_relief_smooth_coastlines(MacroChunk* map, int32_t width_x, int32_t depth_z, int32_t iterations,ZynTestConfig* test_config);

void zyn_gen_map_relief(MacroChunk* map, int32_t width_x, int32_t depth_z, uint32_t seed, ZynTestConfig* test_config);

void normaliser(float* tableau, float min,float max,size_t total_cases,float vmin,float vmax);
/**
 * @brief Génère la carte de hauteur macro-procédurale de Zynthar.
 * * @param hauteurs_triees   [out] Tableau de destination Row-Major (taille: width_x * depth_z).
 * @param masque_voronoi    [in]  Masque d'atténuation de l'île (taille: width_x * depth_z).
 * @param width_x           Largeur de la grille (ex: 2000).
 * @param depth_z           Profondeur de la grille (ex: 1000).
 * @param seed              Graine déterministe globale pour les offsets du monde.
 * @param out_relief_min    [out] Pointeur vers la hauteur minimale consolidée.
 * @param out_relief_max    [out] Pointeur vers la hauteur maximale consolidée.
 */
void zyn_map_relief_perlin(float * __restrict hauteurs_triees,
    const float * __restrict masque_voronoi,
    int32_t width_x,
    int32_t depth_z,
    uint32_t seed,
    float *out_relief_min,
    float *out_relief_max
);
float zyn_niv_mer_corrige(float* hauteurs_triees, size_t total_cases, double max_sea_percentage) ;

void zyn_map_correction_niv_mer(float* hauteurs_triees, size_t total_cases, float niveau_mer_calcule);

void zyn_gen_map_calcul_coin(MacroChunk* map, int32_t width_x, int32_t depth_z,ZynTestConfig* test_config);

#endif /* ZYN_GEN_MAP_RELIEF_H */
