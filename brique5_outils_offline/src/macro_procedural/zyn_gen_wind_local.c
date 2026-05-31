/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 31/05/2026
 *
 * zyn_gen_wind_local  : Implémentation du vent local altéré par la topographie.
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
#include "zyn_gen_wind_local.h"
#include "zyn_gen_wind_global.h"

WindVector zyn_gen_map_wind_local(const MacroChunk* map, int32_t width_x, int32_t depth_z, int32_t chunk_x, int32_t chunk_z) {
    /* 1. Échantillonnage natif du vecteur de vent synoptique global */
    WindVector wind = get_global_wind(chunk_x, chunk_z);
    
    if (map == NULL || width_x <= 0 || depth_z <= 0) return wind;
    if (chunk_x < 0 || chunk_x >= width_x || chunk_z < 0 || chunk_z >= depth_z) return wind;

    size_t idx_centre = (size_t)chunk_z * width_x + chunk_x;
    float alt_centre_m = DM_TO_M(map[idx_centre].elevation_max_dm);

    /* 2. Calcul du gradient topographique local (Voisinage avec clamping aux frontières) */
    int32_t x_ouest = (chunk_x > 0) ? chunk_x - 1 : chunk_x;
    int32_t x_est   = (chunk_x < width_x - 1) ? chunk_x + 1 : chunk_x;
    int32_t z_sud   = (chunk_z > 0) ? chunk_z - 1 : chunk_z;
    int32_t z_nord  = (chunk_z < depth_z - 1) ? chunk_z + 1 : chunk_z;

    float alt_ouest = DM_TO_M(map[(size_t)chunk_z * width_x + x_ouest].elevation_max_dm);
    float alt_est   = DM_TO_M(map[(size_t)chunk_z * width_x + x_est].elevation_max_dm);
    float alt_sud   = DM_TO_M(map[(size_t)z_sud * width_x + chunk_x].elevation_max_dm);
    float alt_nord  = DM_TO_M(map[(size_t)z_nord * width_x + chunk_x].elevation_max_dm);

    /* Vecteur de pente macro (différences centrales partielles) */
    float inv_dx = ((x_est - x_ouest) == 2) ? 0.5f : 1.0f;
    float inv_dz = ((z_nord - z_sud) == 2) ? 0.5f : 1.0f;
    
    float slope_x = (alt_est - alt_ouest) * inv_dx;
    float slope_z = (alt_nord - alt_sud) * inv_dz;

    /* 3. Effets combinés : Friction (plaines/vallées) & Venturi (sommets) */
    float speed_global = sqrtf(wind.dx * wind.dx + wind.dy * wind.dy);
    if (speed_global < 0.001f) return wind;

    /* Le facteur d'altitude module la vitesse :
     * - En dessous du niveau de la mer (canyons/fosses) : friction forte, vitesse divisée par 2 max.
     * - Au-dessus (sommets, max +2048m) : accélération progressive (jusqu'à +40% de gain). */
    float alt_ratio = alt_centre_m / (float)ZYN_WORLD_Y_MAX;
    float modificateur_vitesse = 1.0f + (alt_ratio * 0.40f);
    if (modificateur_vitesse < 0.5f) modificateur_vitesse = 0.5f;

    float speed_locale = speed_global * modificateur_vitesse;

    /* 4. Déviation hydrodynamique : Écoulement tangentiel le long des parois rugueuses */
    /* On normalise le vecteur directionnel du vent d'origine */
    float wx_norm = wind.dx / speed_global;
    float wy_norm = wind.dy / speed_global;

    /* Projection du vent sur le gradient de la pente pour obtenir l'intensité de la collision face au mur */
    float intensite_collision = (wx_norm * slope_x) + (wy_norm * slope_z);

    /* Si le vent percute un mur montant (pente de face positive), on le dévie de manière branchless 
     * en soustrayant une fraction de sa composante frontale pour le forcer à glisser latéralement */
    if (intensite_collision > 0.0f) {
        float attenuation_friction_pente = 1.0f / (1.0f + (intensite_collision * 0.05f));
        speed_locale *= attenuation_friction_pente;

        /* Déviation géométrique sur les axes */
        wx_norm -= slope_x * 0.1f * intensite_collision;
        wy_norm -= slope_z * 0.1f * intensite_collision;

        /* Re-normalisation du vecteur directionnel modifié */
        float nouvelle_norme = sqrtf(wx_norm * wx_norm + wy_norm * wy_norm);
        if (nouvelle_norme > 0.001f) {
            wx_norm /= nouvelle_norme;
            wy_norm /= nouvelle_norme;
        }
    }

    /* Plafond de sécurité physique absolu pour l'univers de Zynthar (Tempête locale max à 140 km/h) */
    if (speed_locale > 140.0f) speed_locale = 140.0f;

    /* Re-projection cartésienne finale */
    WindVector local_wind;
    local_wind.dx = wx_norm * speed_locale;
    local_wind.dy = wy_norm * speed_locale;

    return local_wind;
}
