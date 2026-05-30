/* =============================================================================
 *
 * ZYNTHAR v0.1
 *
 * zyn_gen_map_temperature : Version Ultra-Optimisée (Branchless & Direct Pointer)
 *
 * =============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include <zynthar.h>
#include "zyn_noise.h"
#include "zyn_gen_map_temperature.h"

#define DM_TO_M(dm)  ((float)(dm) / 10.0f)
#define FLOAT_TO_RAW(f)  ((uint8_t)((f) * 255.0f))

void zyn_gen_map_temperature(MacroChunk* map, int32_t width_x, int32_t depth_z) {
    if (map == NULL || width_x <= 0 || depth_z <= 0) return;

    float z_centre = (float)depth_z / 2.0f;

    int32_t octaves = 3;
    float persistence = 0.4f;
    float lacunarity = 2.0f;
    float scale_climat = 0.005f; 
    
    float offset_x = -850.25f;
    float offset_z = 4120.75f;

    for (int32_t z = 0; z < depth_z; z++) {
        float distance_centre = fabsf((float)z - z_centre) / z_centre;
        float gradient_thermique = 1.0f - distance_centre;

        int32_t offset_ligne = z * width_x;
        float nz = ((float)z + offset_z) * scale_climat;

        /* [OPTI] Accès RAM Direct : On pointe directement sur le premier élément de la ligne */
        MacroChunk* chunk = &map[offset_ligne];

        for (int32_t x = 0; x < width_x; x++) {
            float nx = ((float)x + offset_x) * scale_climat;
            float bruit_climat = zyn_fractal_noise2d(nx, nz, octaves, persistence, lacunarity);

            float temp_brute = gradient_thermique + (bruit_climat * 0.20f);

            /* [OPTI] Branchless : On extrait l'altitude et on applique fmaxf pour éliminer le "if" */
            float altitude_m = DM_TO_M(chunk->elevation_max_dm);
            temp_brute -= (fmaxf(0.0f, altitude_m) * 0.0004f); 

            /* Clamping de sécurité */
            if (temp_brute < 0.0f) temp_brute = 0.0f;
            if (temp_brute > 1.0f) temp_brute = 1.0f;
            
            /* Écriture directe via le pointeur */
            chunk->temperature_raw = FLOAT_TO_RAW(temp_brute);

            /* [OPTI] Glissement vers la case mémoire suivante (très efficace pour le cache L1/L2) */
            chunk++;
        }
    }
}
