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

void zyn_gen_map_humidity(MacroChunk* map, int32_t width_x, int32_t depth_z) {
    if (map == NULL || width_x <= 0 || depth_z <= 0) return;

    const int32_t octaves = 4;
    const float persistence = 0.5f;
    const float lacunarity = 2.0f;
    const float scale_climat = 0.007f;
    const float offset_x = 3450.1f;
    const float offset_z = -1280.9f;

    for (int32_t z = 0; z < depth_z; z++) {
        int32_t offset_ligne = z * width_x;
        
        /* Pré-calculs des offsets de lignes pour le gradient (Moore vertical) avec bornage */
        int32_t z_sud  = (z > 0) ? z - 1 : z;
        int32_t z_nord = (z < depth_z - 1) ? z + 1 : z;
        int32_t offset_sud  = z_sud * width_x;
        int32_t offset_nord = z_nord * width_x;

        for (int32_t x = 0; x < width_x; x++) {
            size_t idx_centre = (size_t)offset_ligne + x;
            MacroChunk* chunk = &map[idx_centre];

            /* 1. Base climatique par bruit fractal basse fréquence */
            float nx = ((float)x + offset_x) * scale_climat;
            float nz = ((float)z + offset_z) * scale_climat;
            float bruit_climat = zyn_fractal_noise2d(nx, nz, octaves, persistence, lacunarity);
            float base_humidite = (bruit_climat + 1.0f) * 0.5f;

            /* 2. Pondération thermique (Clausius-Clapeyron simplifié) */
            float ratio_temp = (float)chunk->temperature_raw / 255.0f;
            base_humidite *= (0.4f + 0.6f * ratio_temp);

            /* 3. EFFET OROGRAPHIQUE ULTRA-OPTIMISÉ FUSIONNÉ (Branchless)
             * Échantillonnage unique du vent global (on évite l'appel de fonction et les ré-allocations) */
            WindVector vent = get_global_wind(chunk->chunk_x, chunk->chunk_z);

            /* Calcul local du gradient de pente macro (Moore horizontal) avec bornage */
            int32_t x_ouest = (x > 0) ? x - 1 : x;
            int32_t x_est   = (x < width_x - 1) ? x + 1 : x;

            /* Accès RAM direct ultra-rapide par décalage de pointeur (-1 / +1) */
            float alt_ouest = DM_TO_M((chunk + x_ouest)->elevation_max_dm);
            float alt_est   = DM_TO_M((chunk + x_est)->elevation_max_dm);
            
            /* Accès verticaux via offsets de lignes pré-calculés */
            float alt_sud   = DM_TO_M(map[offset_sud + x].elevation_max_dm);
            float alt_nord  = DM_TO_M(map[offset_nord + x].elevation_max_dm);

            /* Vecteurs du gradient topographique du sol (pente horizontale) */
            float slope_x = (alt_est - alt_ouest) * 0.5f;
            float slope_z = (alt_nord - alt_sud) * 0.5f;

            /* PROUESSES MATHÉMATIQUES BRANCHLESS :
             * Au lieu de faire des "if" sur la direction de provenance du vent et la hauteur,
             * on utilise le produit scalaire direct entre le vent et le gradient de pente.
             * - Si le vent souffle dans le sens montant de la pente, le produit scalaire est POSITIF (Ascendance/Pluie).
             * - Si le vent souffle dans le sens descendant, le produit scalaire est NÉGATIF (Fœhn/Aridité). */
            float produit_scalaire = (vent.dx * slope_x) + (vent.dy * slope_z);

            /* Facteur d'échelle physique pour calibrer l'impact du relief et de la vitesse du courant d'air */
            float effet_orographique = produit_scalaire * 0.00008f;

            /* Application directe et linéaire (Branchless) */
            base_humidite += effet_orographique;

            /* Clamping mathématique rapide sans branchement */
            base_humidite = fmaxf(0.0f, fminf(base_humidite, 1.0f));

            /* Stockage temporaire packagé dans l'octet 'biome' */
            chunk->biome = (uint8_t)roundf(base_humidite * 255.0f);
        }
    }
}
