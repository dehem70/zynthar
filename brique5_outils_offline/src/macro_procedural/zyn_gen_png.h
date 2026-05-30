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

#endif /* ZYN_GEN_PNG_H */
