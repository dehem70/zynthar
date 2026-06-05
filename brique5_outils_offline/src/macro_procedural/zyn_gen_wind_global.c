/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 31/05/2026
 *
 * wind_global : Génération du champ de vent global macro via bruit fractal (FBM).
 * Élimine les bordures géométriques de la grille par superposition d'octaves.
 *
 * =============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _GNU_SOURCE
#include <math.h>

#include <zynthar.h>
#include "zyn_gen_wind_global.h"
#include "zyn_noise.h"

WindVector get_global_wind(int32_t chunk_x, int32_t chunk_y) {
    const float scale = 0.015f; 
    const float fx = (float)chunk_x * scale;
    const float fz = (float)chunk_y * scale;

    const int32_t octaves = 4;
    const float persistence = 0.55f;
    const float lacunarity = 2.15f;
    const float fbm_booster = 2.5f;

    /* =========================================================================
     * OCTAVE 1 : DIRECTION DU VENT
     * ========================================================================= */
    const float noise_angle_raw = zyn_fractal_noise2d(fx + 17.319f, fz + 59.713f, octaves, persistence, lacunarity);
    
    // Simplification algébrique : (clamp(noise * 2.5) + 1.0) * 0.5 * 2 * PI  =>  (clamp(noise * 2.5) + 1.0) * PI
    float noise_angle = noise_angle_raw * fbm_booster;
    noise_angle = __builtin_fmaxf(-1.0f, __builtin_fminf(noise_angle, 1.0f));
    
    const float angle = (noise_angle + 1.0f) * (float)M_PI;

    /* =========================================================================
     * OCTAVE 2 : VITESSE DU VENT
     * ========================================================================= */
    // Constantes de rotation asymétrique pré-calculées
    const float rx = (fx * 0.8660254f) - (fz * 0.5f);
    const float rz = (fx * 0.5f) + (fz * 0.8660254f);

    const float noise_speed_raw = zyn_fractal_noise2d(rx - 143.619f, rz + 87.111f, octaves, persistence, lacunarity);
    
    float noise_speed = noise_speed_raw * fbm_booster;
    noise_speed = __builtin_fmaxf(-1.0f, __builtin_fminf(noise_speed, 1.0f));
    
    // Simplification : (noise_speed + 1.0) * 0.5 * 90.0  =>  (noise_speed + 1.0) * 45.0
    const float speed = (noise_speed + 1.0f) * 45.0f; 

    /* =========================================================================
     * TRANSFORMATION ET SORTIE (FSINCOS COUPLÉ)
     * ========================================================================= */
    float sin_a, cos_a;
    // Appel de l'intrinsèque GCC qui calcule SIN et COS en un seul cycle d'horloge CPU
    __builtin_sincosf(angle, &sin_a, &cos_a);

    WindVector global_wind;
    global_wind.dx = speed * cos_a;
    global_wind.dy = speed * sin_a;

    return global_wind;
}
