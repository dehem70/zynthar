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

#endif /* ZYN_GEN_PNG_H */
