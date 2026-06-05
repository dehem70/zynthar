/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 31/05/2026
 *
 * zyn_gen_map_humidity  : Implémentation du modèle d'humidité atmosphérique globale.
 * utilisation :
 *
 * =============================================================================*/
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

// En-têtes requis sous Linux/POSIX pour manipuler 'struct stat' et 'mkdir'
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <zynthar.h>
#include "zyn_gen_map_humidity.h"
#include "zyn_noise.h"
#include "zyn_gen_wind_global.h"
#include "zyn_utils.h"


void zyn_gen_map_humidity(MacroChunk* __restrict map, int32_t width_x, int32_t depth_z, ZynTestConfig* test_config) {
    if (__builtin_expect((map == NULL || width_x <= 0 || depth_z <= 0), 0)) return;

    const int32_t octaves = 4;
    const float persistence = 0.5f;
    const float lacunarity = 2.0f;
    const float scale_climat = 0.007f;
    const float offset_x = 3450.1f;
    const float offset_z = -1280.9f;
    const size_t w = (size_t)width_x;

    #pragma GCC ivdep
    for (int32_t x = 0; x < width_x; x++) {
        const float nx = ((float)x + offset_x) * scale_climat;
        
        const int32_t x_ouest = (x > 0) ? -1 : 0;
        const int32_t x_est   = (x < width_x - 1) ? 1 : 0;

        #pragma GCC vectorize
        for (int32_t z = 0; z < depth_z; z++) {
            const size_t offset_ligne = (size_t)z * w;
            const size_t idx_centre = offset_ligne + (size_t)x;
            MacroChunk* __restrict chunk = &map[idx_centre];

            /* 1. Base climatique par bruit fractal basse fréquence */
            const float nz = ((float)z + offset_z) * scale_climat;
            const float bruit_climat = zyn_fractal_noise2d(nx, nz, octaves, persistence, lacunarity);
            float base_humidite = (bruit_climat + 1.0f) * 0.5f;

            /* 2. Pondération thermique (Clausius-Clapeyron simplifié) */
            const float ratio_temp = (float)chunk->temperature_raw * (1.0f / 255.0f);
            base_humidite *= (0.4f + 0.6f * ratio_temp);

            /* 3. EFFET OROGRAPHIQUE FLUIDIFIÉ */
            const WindVector vent = get_global_wind(chunk->chunk_x, chunk->chunk_z);

            // Accès ouest/est via les décalages horizontaux branchless sécurisés
            const float alt_ouest = DM_TO_M(chunk[x_ouest].elevation_max_dm);
            const float alt_est   = DM_TO_M(chunk[x_est].elevation_max_dm);
            
            // Bornage vertical strict calculé dynamiquement mais de manière linéaire
            const size_t idx_sud  = ((z > 0) ? offset_ligne - w : offset_ligne) + (size_t)x;
            const size_t idx_nord = ((z < depth_z - 1) ? offset_ligne + w : offset_ligne) + (size_t)x;

            const float alt_sud  = DM_TO_M(map[idx_sud].elevation_max_dm);
            const float alt_nord = DM_TO_M(map[idx_nord].elevation_max_dm);

            /* Vecteurs du gradient topographique du sol */
            const float slope_x = (alt_est - alt_ouest) * 0.5f;
            const float slope_z = (alt_nord - alt_sud) * 0.5f;

            /* Produit scalaire branchless Vent o Pente */
            const float produit_scalaire = (vent.dx * slope_x) + (vent.dy * slope_z);
            const float effet_orographique = produit_scalaire * 0.00008f;

            base_humidite += effet_orographique;

            /* Clamping forcé nativement en instructions SSE/AVX minps/maxps */
            base_humidite = __builtin_fmaxf(0.0f, __builtin_fminf(base_humidite, 1.0f));

            /* Stockage final optimisé via l'intrinsèque de l'arrondi */
            chunk->biome = (uint8_t)__builtin_roundf(base_humidite * 255.0f);
        }
    }

    if (__builtin_expect((test_config != NULL && test_config->active_test == 1 && test_config->target_step == 6), 0)) {
        test_config->early_exit = 1;
    }
}

