#ifndef ZYN_GEN_PNG_H
#define ZYN_GEN_PNG_H

#include <stdint.h>

// Inclusion globale gérée par CMake
#include <zynthar.h>

/**
 * @brief Exporte la carte d'élévation macro en une image PNG en niveaux de gris et son masque binaire.
 * L'altitude -100m (ou moins) sera noire (0), l'altitude +100m (ou plus) sera blanche (255).
 * Le masque binaire dissocie la terre (blanc) de la mer (noir).
 *
 * @param map Pointeur vers la grille de MacroChunks.
 * @param width_x Largeur transversale de la carte (Axe X).
 * @param depth_z Longueur longitudinale de la carte (Axe Z).
 * @param filename Chemin du fichier PNG de relief à générer (ex: "carte_elevation.png").
 * @param filename_bin Chemin du fichier PNG du masque binaire (ex: "carte_masque_mer.png").
 * @return int 1 si les fichiers ont été écrits avec succès, 0 en cas d'échec.
 */
int zyn_gen_png_elevation(const MacroChunk* map, int32_t width_x, int32_t depth_z, const char* filename, const char* filename_bin);

/**
 * @brief Exporte la carte des températures macro en une image PNG en niveaux de gris.
 * Le froid polaire (0) sera noir (0), la chaleur équatoriale (255) sera blanche (255).
 *
 * @param map Pointeur constant vers la grille de MacroChunks.
 * @param width_x Largeur transversale de la carte (Axe X).
 * @param depth_z Longueur longitudinale de la carte (Axe Z).
 * @param filename Chemin du fichier PNG à générer (ex: "carte_temperature.png").
 * @return int 1 si le fichier a été écrit avec succès, 0 en cas d'échec.
 */
int zyn_gen_png_temperature(const MacroChunk* map, int32_t width_x, int32_t depth_z, const char* filename);  

/**
 * @brief Exporte la carte des vecteurs de vent sous forme de grille de flèches (Quiver Plot).
 * Dessine des segments orientés par-dessus la carte de vitesse en niveaux de gris.
 *
 * @param map Pointeur constant vers la grille de MacroChunks.
 * @param width_x Largeur transversale de la carte (Axe X).
 * @param depth_z Longueur longitudinale de la carte (Axe Z).
 * @param filename Chemin du fichier PNG vectoriel à générer (ex: "carte_vent_vecteurs.png").
 * @return int 1 si le fichier a été écrit avec succès, 0 en cas d'échec.
 */
int zyn_gen_png_wind_vectors(const MacroChunk* map, int32_t width_x, int32_t depth_z, const char* filename);

/**
 * @brief Exporte la carte du vent local sous forme de grille de flèches (Quiver Plot) superposée au relief.
 * Le fond affiche la topographie amortie, les flèches blanches montrent les déviations et accélérations.
 *
 * @param map Pointeur constant vers la grille de MacroChunks.
 * @param width_x Largeur transversale de la carte (Axe X).
 * @param depth_z Longueur longitudinale de la carte (Axe Z).
 * @param filename Chemin du fichier PNG vectoriel local à générer (ex: "carte_vent_local_vecteurs.png").
 * @return int 1 si le fichier a été écrit avec succès, 0 en cas d'échec.
 */
int zyn_gen_png_wind_local_vectors(const MacroChunk* map, int32_t width_x, int32_t depth_z, const char* filename);

/**
 * @brief Exporte la carte d'humidité macro stockée temporairement dans le champ biome.
 * La sécheresse absolue (0) sera noire, les précipitations maximales (255) seront blanches.
 *
 * @param map Pointeur constant vers la grille de MacroChunks.
 * @param width_x Largeur transversale de la carte.
 * @param depth_z Longueur longitudinale de la carte.
 * @param filename Chemin du fichier PNG à générer (ex: "carte_humidite.png").
 * @return int 1 si le fichier a été écrit avec succès, 0 en cas d'échec.
 */
int zyn_gen_png_humidity(const MacroChunk* map, int32_t width_x, int32_t depth_z, const char* filename);

/**
 * @brief Exporte la carte hydrographique à l'échelle fine (Micro-Chunks) superposée au relief.
 * Les lits des rivières s'éclairent proportionnellement à la force de leur débit.
 *
 * @param map Pointeur constant vers la grille de MacroChunks.
 * @param width_x Largeur transversale de la carte.
 * @param depth_z Longueur longitudinale de la carte.
 * @param flux_grid Grille de flux micro calculée (taille width_x * depth_z * 400).
 * @param filename Chemin du fichier PNG hydrographique à générer.
 * @return int 1 si le fichier a été écrit avec succès, 0 en cas d'échec.
 */
int zyn_gen_png_rivers(const MacroChunk* map, int32_t width_x, int32_t depth_z, const uint32_t* flux_grid, const char* filename);

/**
 * @brief Exporte la carte des biomes.
 *
 * @param map Pointeur constant vers la grille de MacroChunks.
 * @param width_x Largeur transversale de la carte.
 * @param depth_z Longueur longitudinale de la carte.
 * @param filename Chemin du fichier PNG hydrographique à générer.
 * @return int 1 si le fichier a été écrit avec succès, 0 en cas d'échec.
 */

int zyn_gen_png_biomes(const MacroChunk* map, int32_t width_x, int32_t depth_z, const char* filename);
#endif /* ZYN_GEN_PNG_H */
