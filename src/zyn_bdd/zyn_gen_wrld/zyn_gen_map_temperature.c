//////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                      //
//                                          ZYNTHAR v0.1                                                //
//                                                                                                      //
// Auteur : Dehem70                                                                                     //
// Date   : 29/05/2026                                                                                  //
//                                                                                                      //
// zyn_gen_map_temperature  ; génération de la température sur la carte                                 //
// utilisation :                                                                                        //
//                                                                                                      //
//                                                                                                      //
//////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zyn_gen_map_temperature.h"
#include "zyn_noise.h"
#include <math.h>

void zyn_gen_map_temperature(MacroChunk* map, int32_t width, int32_t height) {
    if (map == NULL || width <= 0 || height <= 0) return;

    float y_centre = (float)height / 2.0f;

    /* Configuration d'un bruit climatique doux et étalé */
    int32_t octaves = 3;
    float persistence = 0.4f;
    float lacunarity = 2.0f;
    float scale_climat = 0.005f; /* Fréquence basse pour de grandes zones */
    
    /* Offsets déterministes pour séparer ce bruit de celui du relief */
    float offset_x = -850.25f;
    float offset_y = 4120.75f;

    for (int32_t y = 0; y < height; y++) {
        /* 1. Calcul du gradient planétaire en sandwich (0.0 aux pôles, 1.0 à l'équateur) */
        float distance_centre = fabsf((float)y - y_centre) / y_centre;
        float gradient_thermique = 1.0f - distance_centre;

        for (int32_t x = 0; x < width; x++) {
            /* 2. Échantillonnage du bruit fractal continu */
            float nx = ((float)x + offset_x) * scale_climat;
            float ny = ((float)y + offset_y) * scale_climat;
            float bruit_climat = zyn_fractal_noise2d(nx, ny, octaves, persistence, lacunarity);

            /* 3. Fusion : Le gradient est perturbé à hauteur de 20% par le bruit */
            float temp_brute = gradient_thermique + (bruit_climat * 0.20f);

            /* 3b. Influence de l'altitude : plus on est haut, plus il fait froid */
            int32_t index = y * width + x;
            float altitude = map[index].elevation_max;
            if (altitude > 0.0f) {
            	/* Coefficient de 0.4f : les hautes montagnes seront très froides */
                temp_brute -= (altitude * 0.40f); 
            }

            /* 4. Clamping de sécurité strict entre [0.0f, 1.0f] */
            temp_brute = fmaxf(-1.0f, fminf(temp_brute, 1.0f));
            
            /* 5. Écriture directe in-place dans la grille */
            map[index].temperature = temp_brute;
        }
    }
}


