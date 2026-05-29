//////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                      //
//                                          ZYNTHAR v0.1                                                //
//                                                                                                      //
// Auteur : Dehem70                                                                                     //
// Date   : 28/05/2026                                                                                  //
//                                                                                                      //
// zyn_gen_png  ; convertir les cartes en images                                                        //
// utilisation :                                                                                        //
//                                                                                                      //
//                                                                                                      //
//////////////////////////////////////////////////////////////////////////////////////////////////////////

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../include/stb_image_write.h"
#include "zyn_gen_png.h"
#include <stdlib.h>
#include <stdint.h>

int zyn_gen_png_elevation(const MacroChunk* map, int32_t width, int32_t height, const char* filename,const char* filename_bin) {
    if (map == NULL || width <= 0 || height <= 0 || filename == NULL) return 0;

    size_t total_pixels = (size_t)width * (size_t)height;
    
    /* Allocation du buffer de pixels (1 octet par pixel pour du niveau de gris) */
    uint8_t* pixels = (uint8_t*)malloc(total_pixels * sizeof(uint8_t));
    uint8_t* pixels_bin = (uint8_t*)malloc(total_pixels * sizeof(uint8_t));
    if (pixels == NULL) return 0;

    /* Conversion des altitudes float [-1.0, 1.0] en octets [0, 255] */
    for (size_t i = 0; i < total_pixels; i++) {
        float alt = map[i].elevation_max;

        /* Clamping de sécurité pour ne pas déborder des limites [-1.0, 1.0] */
        if (alt < -1.0f) alt = -1.0f;
        if (alt > 1.0f)  alt = 1.0f;

        /* Normalisation linéaire entre 0 et 255 */
        float normalisee = (alt + 1.0f) / 2.0f;
        pixels[i] = (uint8_t)(normalisee * 255.0f);
        if (alt < 0.0f) {
          pixels_bin[i] = (uint8_t)(0.0f);
        }
        else {
          pixels_bin[i] = (uint8_t)(255.0f);
        }
        
    }

    /* Écriture du fichier PNG via STB 
       Le paramètre '1' indique 1 canal de couleur (Grayscale)
       Le paramètre 'width' à la fin correspond au "stride" en octets d'une ligne d'image */
    int resultat = stbi_write_png(filename, width, height, 1, pixels, width);
    int resultat_bin = stbi_write_png(filename_bin, width, height, 1, pixels_bin, width);
    
    /* Libération de la mémoire du buffer d'image */
    free(pixels);
    free(pixels_bin);
    
    return resultat*resultat_bin;
}

int zyn_gen_png_temperature(const MacroChunk* map, int32_t width, int32_t height, const char* filename) {
    if (map == NULL || width <= 0 || height <= 0 || filename == NULL) return 0;
    size_t total_pixels = (size_t)width * (size_t)height;
    uint8_t* pixels = (uint8_t*)malloc(total_pixels * sizeof(uint8_t));
    if (pixels == NULL) return 0;
    for (size_t i = 0; i < total_pixels; i++) {
        float temp = map[i].temperature;
        /* Comme notre température est déjà strictement entre 0.0f et 1.0f,
           la conversion en octet [0, 255] est directe et ultra-propre */
        pixels[i] = (uint8_t)(temp * 255.0f);
    }

    int resultat = stbi_write_png(filename, width, height, 1, pixels, width);
     free(pixels);
    return resultat;
}
