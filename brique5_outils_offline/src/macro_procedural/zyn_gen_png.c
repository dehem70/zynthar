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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <zynthar.h>
#include "zyn_gen_png.h"
#include "zyn_river_agent.h"

// On suppose que la lib stb est incluse via ton implémentation locale
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h" 

/* =============================================================================
 * RENDU DU RELIEF (1 Canal - Niveaux de gris)
 * ============================================================================= */
int zyn_gen_png_elevation(const MacroChunk* map, int32_t width_x, int32_t depth_z, const char* filename) {
if (map == NULL || width_x <= 0 || depth_z <= 0 || filename == NULL) return 0;

    size_t total_pixels = (size_t)width_x * (size_t)depth_z;
    uint8_t* pixels = (uint8_t*)malloc(total_pixels * sizeof(uint8_t));
    if (pixels == NULL) return 0;

    const float alt_min = (float)ZYN_WORLD_Y_MIN;
    const float alt_max = (float)ZYN_WORLD_Y_MAX;
    const float range = alt_max - alt_min;

    /* RECONSTRUCTION EN DOUBLE BOUCLE 2D EXPLICITE POUR EVITER TOUT ARRONDI ASYMETRIQUE CENTRAL */
    for (int32_t z = 0; z < depth_z; z++) {
        size_t offset_ligne = (size_t)z * (size_t)width_x;
        for (int32_t x = 0; x < width_x; x++) {
            size_t index = ZYN_INDEX(x, z, width_x);

            /* Récupération et conversion ultra-sécurisée */
            float alt_m = (float)map[index].elevation_max_dm / 10.0f;
            
            /* Normalisation avec protection stricte des bornes */
            float normalisee = (alt_m - alt_min) / range;
            if (normalisee > 1.0f) normalisee = 1.0f;
            if (normalisee < 0.0f) normalisee = 0.0f;

            /* Utilisation de lrintf pour un arrondi bancaire stable au niveau des bits du CPU */
            int32_t pixel_val = lrintf(normalisee * 255.0f);
            if (pixel_val < 0)   pixel_val = 0;
            if (pixel_val > 255) pixel_val = 255;

            pixels[index] = (uint8_t)pixel_val;
        }
    }

    /* Encodage PNG brut sans contrainte de stride */
    int resultat = stbi_write_png(filename, width_x, depth_z, 1, pixels, 0);
    free(pixels);
    return resultat;
}
/* =============================================================================
 * RENDU DE LA TEMPERATURE (1 Canal - Niveaux de gris)
 * ============================================================================= */
int zyn_gen_png_temperature(const MacroChunk* map, int32_t width_x, int32_t depth_z, const char* filename) {
    if (map == NULL || width_x <= 0 || depth_z <= 0 || filename == NULL) return 0;

    size_t total_pixels = (size_t)width_x * (size_t)depth_z;
    uint8_t* pixels = (uint8_t*)malloc(total_pixels * sizeof(uint8_t));
    if (pixels == NULL) return 0;

    for (size_t i = 0; i < total_pixels; i++) {
        pixels[i] = map[i].temperature_raw;
    }

    /* Même chose ici, stride à 0 */
    int resultat = stbi_write_png(filename, width_x, depth_z, 1, pixels, 0);
    free(pixels);
    return resultat;
}

/* =============================================================================
 * RENDU DE L'HUMIDITE (1 Canal - Niveaux de gris)
 * ============================================================================= */
int zyn_gen_png_humidite(const MacroChunk* map, int32_t width_x, int32_t depth_z, const char* filename) {
    if (map == NULL || width_x <= 0 || depth_z <= 0 || filename == NULL) return 0;

    size_t total_pixels = (size_t)width_x * (size_t)depth_z;
    uint8_t* pixels = (uint8_t*)malloc(total_pixels * sizeof(uint8_t));
    if (pixels == NULL) return 0;

    for (size_t i = 0; i < total_pixels; i++) {
        pixels[i] = map[i].biome;
    }

    /* Même chose ici, stride à 0 */
    int resultat = stbi_write_png(filename, width_x, depth_z, 1, pixels, 0);
    free(pixels);
    return resultat;
}

/* =============================================================================
 * RENDU DES RIVIERES ET HYDROGRAPHIE (1 Canal)
 * ============================================================================= */
/*int zyn_gen_png_rivers(const MacroChunk* map, int32_t width_x, int32_t depth_z, const uint32_t* flux_grid, const char* filename) {
    if (map == NULL || width_x <= 0 || depth_z <= 0 || flux_grid == NULL || filename == NULL) return 0;

    size_t total_pixels = (size_t)width_x * (size_t)depth_z;
    uint8_t* pixels = (uint8_t*)malloc(total_pixels * sizeof(uint8_t));
    if (pixels == NULL) return 0;

    const float alt_min_rendu = (float)ZYN_WORLD_Y_MIN;
    const float range_rendu = (float)ZYN_WORLD_Y_MAX - alt_min_rendu;

    for (size_t i = 0; i < total_pixels; i++) {
        float alt_m = DM_TO_M(map[i].elevation_max_dm);
        float normalisee = (alt_m - alt_min_rendu) / range_rendu;
        if (normalisee > 1.0f) normalisee = 1.0f;
        if (normalisee < 0.0f) normalisee = 0.0f;

        pixels[i] = (uint8_t)(normalisee * 255.0f * 0.15f); 
    }

    for (size_t i = 0; i < total_pixels; i++) {
        if (map[i].biome == 255 || map[i].biome == 253) {
            pixels[i] = 255; 
            continue;
        }

        uint32_t flux = flux_grid[i];
        if (flux > 0) {
            uint32_t intensite = 130 + (flux * 4);
            if (intensite > 240) intensite = 240;
            pixels[i] = (uint8_t)intensite;
        }
    }

    /* Stride à 0 */
  /*  int resultat = stbi_write_png(filename, width_x, depth_z, 1, pixels, 0);
    free(pixels);
    return resultat;
}
*/

int zyn_gen_png_rivers(const MacroChunk* map, int32_t width_x, int32_t depth_z, 
                       const ZynRiverNode* river_nodes, int32_t nodes_count, 
                       const char* filename) 
{
    if (map == NULL || width_x <= 0 || depth_z <= 0 || river_nodes == NULL || filename == NULL) return 0;

    size_t total_pixels = (size_t)width_x * (size_t)depth_z;
    uint8_t* pixels = (uint8_t*)malloc(total_pixels * 3 * sizeof(uint8_t));
    if (pixels == NULL) return 0;

//    const float alt_min_rendu = (float)ZYN_WORLD_Y_MIN;
    const float alt_min_rendu = 0;
    const float range_rendu = (float)ZYN_WORLD_Y_MAX - alt_min_rendu;

    // 1. Fond de carte
    for (size_t i = 0; i < total_pixels; i++) {
        size_t idx_rgb = i * 3;

        if (map[i].biome == 255 || map[i].biome == 253) {
            pixels[idx_rgb + 0] = 15;   pixels[idx_rgb + 1] = 45;   pixels[idx_rgb + 2] = 120;
            continue;
        }

        float alt_m = DM_TO_M(map[i].elevation_max_dm);
        float normalisee = (alt_m - alt_min_rendu) / range_rendu;
        if (normalisee > 1.0f) normalisee = 1.0f;
        if (normalisee < 0.0f) normalisee = 0.0f;

        uint8_t facteur_relief = (uint8_t)(normalisee * 255.0f);
        pixels[idx_rgb + 0] = (uint8_t)(30 + (facteur_relief * 0.4f));
        pixels[idx_rgb + 1] = (uint8_t)(110 - (facteur_relief * 0.2f));
        pixels[idx_rgb + 2] = 40;
    }

    // 2. Application des rivières extraites par-dessus
    for (int32_t i = 0; i < nodes_count; i++) {
        int32_t rx = river_nodes[i].macro_x;
        int32_t rz = river_nodes[i].macro_z;

        if (rx < 0 || rx >= width_x || rz < 0 || rz >= depth_z) continue;

        // C'est le SEUL endroit du projet où le * 3 est légitime !
        size_t pixel_index = ((size_t)rz * (size_t)width_x + (size_t)rx) * 3;

        uint32_t flux = river_nodes[i].flow_volume;
        uint32_t intensite_bleu = 160 + (flux / 10); // Ajustement pour éviter l'overflow visuel
        if (intensite_bleu > 255) intensite_bleu = 255;

        pixels[pixel_index + 0] = 0;
        pixels[pixel_index + 1] = (uint8_t)(intensite_bleu - 30);
        pixels[pixel_index + 2] = (uint8_t)intensite_bleu;
    }

    int resultat = stbi_write_png(filename, width_x, depth_z, 3, pixels, (int)(width_x * 3));
    free(pixels);
    return resultat;
}

/* =============================================================================
 * RENDU COULEUR DES BIOMES (3 Canaux - RGB)
 * ============================================================================= */
int zyn_gen_png_biomes(const MacroChunk* map, int32_t width_x, int32_t depth_z, const char* filename) {
    if (map == NULL || width_x <= 0 || depth_z <= 0 || filename == NULL) return 0;

    size_t total_pixels = (size_t)width_x * (size_t)depth_z;
    uint8_t* pixels = (uint8_t*)malloc(total_pixels * 3 * sizeof(uint8_t));
    if (pixels == NULL) return 0;

    for (size_t i = 0; i < total_pixels; i++) {
        size_t p_idx = i * 3;
        uint8_t b_id = map[i].biome;
        uint8_t r = 0, g = 0, b = 0;

        switch (b_id) {
            case ZYN_BIOME_ABYSSE:            r = 5;   g = 15;  b = 95;   break;
            case ZYN_BIOME_EAU_PROFONDE:      r = 15;  g = 40;  b = 95;   break;
            case ZYN_BIOME_EAU_COTIERE:       r = 40;  g = 130; b = 190;  break;
            case ZYN_BIOME_EAU_INTERIEURE:    r = 45;  g = 100; b = 150;  break;
            case ZYN_BIOME_PLAGE:             r = 230; g = 205; b = 140;  break;
            case ZYN_BIOME_DESERT:            r = 215; g = 155; b = 85;   break;
            case ZYN_BIOME_PLAINE:            r = 160; g = 195; b = 105;  break;
            case ZYN_BIOME_FORET:             r = 50;  g = 135; b = 70;   break;
            case ZYN_BIOME_TAIGA:             r = 35;  g = 85;  b = 65;   break;
            case ZYN_BIOME_TOUNDRA:           r = 135; g = 145; b = 130;  break;
            case ZYN_BIOME_JUNGLE:            r = 10;  g = 95;  b = 40;   break;
            case ZYN_BIOME_GLACIER:           r = 195; g = 225; b = 235;  break;
            case ZYN_BIOME_MONTAGNE_ROCHEUSE: r = 110; g = 110; b = 115;  break;
            case ZYN_BIOME_PIC_ENNEIGE:       r = 245; g = 245; b = 250;  break;
            default:                          r = 0;   g = 0;   b = 0;     break;
        }

        pixels[p_idx]     = r;
        pixels[p_idx + 1] = g;
        pixels[p_idx + 2] = b;
    }

    /* Pour le RGB à 3 canaux, passer 0 sécurise totalement l'étalement des lignes */
    int resultat = stbi_write_png(filename, width_x, depth_z, 3, pixels, 0);
    free(pixels);
    return resultat;
}
