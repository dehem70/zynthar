#ifndef ZYN_GEN_PNG_H
#define ZYN_GEN_PNG_H

#include "../../include/zynthar.h"

/**
 * @brief Exporte la carte d'élévation macro en une image PNG en niveaux de gris.
 * L'altitude -1.0f (ou moins) sera noire (0), l'altitude 1.0f (ou plus) sera blanche (255).
 * * @param map Pointeur vers la grille de MacroChunks.
 * @param width Largeur de la carte.
 * @param height Hauteur de la carte.
 * @param filename Chemin du fichier PNG à générer (ex: "carte_elevation.png").
 * @return int 1 si le fichier a été écrit avec succès, 0 en cas d'échec.
 */
int zyn_gen_png_elevation(const MacroChunk* map, int32_t width, int32_t height, const char* filename,const char* filename_bin);

/**
 * @brief Exporte la carte des températures macro en une image PNG en niveaux de gris.
 * * La température minimale (0.0f, froid polaire) sera noire (0),
 * la température maximale (1.0f, chaleur équatoriale) sera blanche (255).
 * Les chaînes de montagnes et les perturbations du bruit y apparaîtront visuellement.
 * * @param map Pointeur constant vers la grille de MacroChunks.
 * @param width Largeur de la carte (X).
 * @param height Hauteur de la carte (Y).
 * @param filename Chemin du fichier PNG à générer (ex: "carte_temperature.png").
 * @return int 1 si le fichier a été écrit avec succès, 0 en cas d'échec (ex: pointeur NULL, erreur d'allocation).
*/
int zyn_gen_png_temperature(const MacroChunk* map, int32_t width, int32_t height, const char* filename);  

#endif /* ZYN_GEN_PNG_H */
