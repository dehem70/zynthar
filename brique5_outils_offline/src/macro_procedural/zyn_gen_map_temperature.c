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

    /* Extraction des constantes physiques de zynthar.h */
    float temp_min_monde = (float)ZYN_WORLD_TEMP_MIN; // -25.0f
    float temp_max_monde = (float)ZYN_WORLD_TEMP_MAX; //  45.0f
    float plage_thermique = temp_max_monde - temp_min_monde; // 70.0°C de dynamique

    for (int32_t z = 0; z < depth_z; z++) {
        float distance_centre = fabsf((float)z - z_centre) / z_centre;
        float gradient_thermique = 1.0f - distance_centre; // 1.0 au centre (Équateur), 0.0 aux bords (Pôles)

        int32_t offset_ligne = z * width_x;
        float nz = ((float)z + offset_z) * scale_climat;

        MacroChunk* chunk = &map[offset_ligne];
        
        for (int32_t x = 0; x < width_x; x++) {
            float nx = ((float)x + offset_x) * scale_climat;
            float bruit_climat = zyn_fractal_noise2d(nx, nz, octaves, persistence, lacunarity);

            /* =========================================================================
             * ÉTAPE 1 : CALCUL DE LA VALEUR BRUTE ABSTRAITE [0.0f, 1.0f] (Niveau Mer)
             * ========================================================================= */
            float base_abstraite = gradient_thermique + (bruit_climat * 0.20f);
            
            // Clamping de sécurité pour l'espace abstrait avant conversion
            base_abstraite = fmaxf(0.0f, fminf(base_abstraite, 1.0f));

            /* =========================================================================
             * ÉTAPE 2 : PROJECTION DANS L'ESPACE PHYSIQUE (°C REELS)
             * ========================================================================= */
            // On convertit la base en vrais degrés Celsius au niveau de la mer
            float temp_physique_c = temp_min_monde + (base_abstraite * plage_thermique);

            // COUPLAGE RELIEF : Perte standard de -0.0065°C par mètre d'altitude (Loi physique réelle)
            // On ne refroidit que si on est au-dessus du niveau de la mer (altitude > 0)
            float altitude_m = DM_TO_M(chunk->elevation_max_dm)-ZYN_SEA_LEVEL;
            float perte_altitude = fmaxf(0.0f, altitude_m) * 0.0065f; 
            
            temp_physique_c -= perte_altitude;

            // Clamping physique strict selon les bornes de ton univers définies dans zynthar.h
            temp_physique_c = fmaxf(temp_min_monde, fminf(temp_physique_c, temp_max_monde));
            
            /* =========================================================================
             * ÉTAPE 3 : NORMALISATION FINALE POUR LE STOCKAGE (0 à 255)
             * ========================================================================= */
            // On calcule le ratio physique final du pixel [0.0f, 1.0f]
            float ratio_final = (temp_physique_c - temp_min_monde) / plage_thermique;

            // Encodage propre dans l'octet (0 = -25°C, 255 = +45°C)
            chunk->temperature_raw = (uint8_t)roundf(ratio_final * 255.0f);

            chunk++;
        }
    }
}
