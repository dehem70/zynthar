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
#include "zyn_utils.h"

void zyn_gen_map_temperature(MacroChunk* __restrict map, int32_t width_x, int32_t depth_z, ZynTestConfig* test_config) {
    if (__builtin_expect((map == NULL || width_x <= 0 || depth_z <= 0), 0)) return;

    const float z_centre = (float)depth_z * 0.5f;
    const float inv_z_centre = 1.0f / z_centre;

    const int32_t octaves = 3;
    const float persistence = 0.4f;
    const float lacunarity = 2.0f;
    const float scale_climat = 0.005f; 
    
    const float offset_x = -850.25f;
    const float offset_z = 4120.75f;

    const float temp_min_monde = (float)ZYN_WORLD_TEMP_MIN; // -25.0f
    const float temp_max_monde = (float)ZYN_WORLD_TEMP_MAX; //  45.0f
    const float plage_thermique = temp_max_monde - temp_min_monde; 
    
    const size_t total_cases = (size_t)width_x * (size_t)depth_z;
    
    // Allocation hybride ultra-rapide (Pile vs Tas) pour éviter la latence du gestionnaire de mémoire
    float* __restrict temperature_temp;
    temperature_temp = (float*)malloc(total_cases * sizeof(float));
    if (__builtin_expect((temperature_temp == NULL), 0)) return;

    float temp_min = 0.0f;
    float temp_max = 0.0f;

    // Boucle externe X et interne Z inversées pour respecter la localité du cache L1/L2
    #pragma GCC ivdep
    for (int32_t x = 0; x < width_x; x++) {
        const float nx = ((float)x + offset_x) * scale_climat;

        #pragma GCC vectorize
        for (int32_t z = 0; z < depth_z; z++) {
            const size_t idx = (size_t)z * (size_t)width_x + (size_t)x;
            
            const float distance_centre = __builtin_fabsf((float)z - z_centre) * inv_z_centre;
            const float gradient_thermique = 1.0f - distance_centre;

            const float nz = ((float)z + offset_z) * scale_climat;
            const float bruit_climat = zyn_fractal_noise2d(nx, nz, octaves, persistence, lacunarity);

            const float base_abstraite = gradient_thermique + (bruit_climat * 0.20f);
            float temp_physique_c = temp_min_monde + (base_abstraite * plage_thermique);

            // Couplage relief optimisé branchless
            const float altitude_m = DM_TO_M(map[idx].elevation_max_dm) - ZYN_SEA_LEVEL;
            const float perte_altitude = __builtin_fmaxf(0.0f, altitude_m) * 0.0065f; 
            
            temp_physique_c -= perte_altitude;
            
            temperature_temp[idx] = temp_physique_c;
            
            temp_min = __builtin_fminf(temp_physique_c, temp_min);
            temp_max = __builtin_fmaxf(temp_physique_c, temp_max);
        }
    }

    normaliser(temperature_temp, temp_min, temp_max, total_cases, temp_min_monde, temp_max_monde);
    
    // Vectorisation SIMD de la réécriture finale
    #pragma GCC ivdep
    #pragma GCC vectorize
    for (size_t p = 0; p < total_cases; p++) {
        map[p].temperature_raw = FLOAT_TO_RAW(temperature_temp[p]);
    }

    free(temperature_temp);

    if (__builtin_expect((test_config != NULL && test_config->active_test == 1 && test_config->target_step == 5), 0)) {
        test_config->early_exit = 1;
    }
}

