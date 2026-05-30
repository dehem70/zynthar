/* =============================================================================
 *
 * ZYNTHAR v0.1
 *
 * Auteur : Dehem70
 * Date   : 28/05/2026
 *
 * zyn_gen_png : Convertisseur de grilles MacroChunks en fichiers images PNG (Grayscale)
 * Utilise la bibliothèque standalone stb_image_write.
 * Aligné sur l'axe horizontal longitudinal Z et adapté aux structures compressées.
 *
 * =============================================================================*/

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <zynthar.h>
#include "zyn_gen_png.h"

// Outil de conversion d'unités (Décimètres -> Mètres)
#define DM_TO_M(dm)  ((float)(dm) / 10.0f)

/* =============================================================================
 * EXPORTATION DE LA CARTE DE RELIEF (NIVEAU DE GRIS & MASQUE TERRE/MER)
 * ============================================================================= */

int zyn_gen_png_elevation(const MacroChunk* map, int32_t width_x, int32_t depth_z, const char* filename, const char* filename_bin) {
    if (map == NULL || width_x <= 0 || depth_z <= 0 || filename == NULL || filename_bin == NULL) return 0;

    size_t total_pixels = (size_t)width_x * (size_t)depth_z;
    
    /* Allocation des buffers d'images (1 canal de gris = 1 octet par pixel) */
    uint8_t* pixels = (uint8_t*)malloc(total_pixels * sizeof(uint8_t));
    uint8_t* pixels_bin = (uint8_t*)malloc(total_pixels * sizeof(uint8_t));
    if (pixels == NULL || pixels_bin == NULL) {
        free(pixels);
        free(pixels_bin);
        return 0;
    }

    /* Plage dynamique d'étalement pour le rendu visuel (en mètres physiques) */
    const float alt_min_rendu = ZYN_WORLD_Y_MIN;
    const float alt_max_rendu = ZYN_WORLD_Y_MAX;
    const float range_rendu = alt_max_rendu - alt_min_rendu;

    for (size_t i = 0; i < total_pixels; i++) {
        /* 1. Décompression de l'altitude du chunk (décimètres -> mètres) */
        float alt_m = DM_TO_M(map[i].elevation_max_dm);

        /* 2. Clamping visuel strict pour calibrer la dynamique du niveau de gris */
        if (alt_m < alt_min_rendu) alt_m = alt_min_rendu;
        if (alt_m > alt_max_rendu) alt_m = alt_max_rendu;

        /* 3. Normalisation linéaire entre [0.0f, 1.0f] puis passage sur 8 bits [0, 255] */
        float normalisee = (alt_m - alt_min_rendu) / range_rendu;
        pixels[i] = (uint8_t)(normalisee * 255.0f);

        /* 4. [OPTI Branchless] Génération du masque binaire terre/mer
         * On utilise le signe de l'altitude packagée : 
         * Si max_elevation > 0 (terre), la condition vaut 1, le pixel prend 255 (blanc).
         * Si max_elevation <= 0 (mer), la condition vaut 0, le pixel prend 0 (noir). */
        pixels_bin[i] = (map[i].elevation_max_dm > ZYN_SEA_LEVEL) ? 255 : 0;
    }

    /* Écriture des deux fichiers PNG via STB
       Le paramètre '1' indique un canal unique (Grayscale).
       Le dernier paramètre correspond au stride (largeur de ligne en octets, ici width_x * 1). */
    int resultat = stbi_write_png(filename, width_x, depth_z, 1, pixels, width_x);
    int resultat_bin = stbi_write_png(filename_bin, width_x, depth_z, 1, pixels_bin, width_x);
    
    /* Libération propre des buffers */
    free(pixels);
    free(pixels_bin);
    
    /* Retourne 1 uniquement si les deux fichiers ont été écrits avec succès sur le disque */
    return (resultat && resultat_bin);
}

/* =============================================================================
 * EXPORTATION DE LA CARTE DE TEMPERATURE (COPIE BRUTE ZERO-CONVERSION)
 * ============================================================================= */

int zyn_gen_png_temperature(const MacroChunk* map, int32_t width_x, int32_t depth_z, const char* filename) {
    if (map == NULL || width_x <= 0 || depth_z <= 0 || filename == NULL) return 0;
    
    size_t total_pixels = (size_t)width_x * (size_t)depth_z;
    uint8_t* pixels = (uint8_t*)malloc(total_pixels * sizeof(uint8_t));
    if (pixels == NULL) return 0;

    /* [OPTI Maximale] La température étant déjà encodée sur un octet natif uint8_t [0, 255],
     * il n'y a plus aucun calcul de conversion ni flottant à traiter. 
     * C'est un simple streaming de données direct depuis la RAM. */
    for (size_t i = 0; i < total_pixels; i++) {
        pixels[i] = map[i].temperature_raw;
    }

    int resultat = stbi_write_png(filename, width_x, depth_z, 1, pixels, width_x);
    
    free(pixels);
    return resultat;
}
