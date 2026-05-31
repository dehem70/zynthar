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
#include <math.h>

#include <zynthar.h>
#include "zyn_gen_wind_global.h"
#include "zyn_noise.h"

WindVector get_global_wind(int32_t chunk_x, int32_t chunk_y) {
    /* Échelle à très basse fréquence pour simuler les grands courants */
    const float scale = 0.015f; 
    float fx = (float)chunk_x * scale;
    float fz = (float)chunk_y * scale;

    /* Configuration des caractéristiques fractales pour un rendu fluide */
    const int32_t octaves = 4;
    const float persistence = 0.55f;
    const float lacunarity = 2.15f;

    /* Constante d'amplification pour compenser l'atténuation native des FBM */
    const float fbm_booster = 2.5f;

    /* =========================================================================
     * OCTAVE 1 : DIRECTION DU VENT (FBM Fractale)
     * ========================================================================= */
    float nx_angle = fx + 17.319f;
    float nz_angle = fz + 59.713f;
    
    float noise_angle = zyn_fractal_noise2d(nx_angle, nz_angle, octaves, persistence, lacunarity);
    
    /* Amplification et clamping strict dans [-1.0f, 1.0f] */
    noise_angle *= fbm_booster;
    noise_angle = fmaxf(-1.0f, fminf(noise_angle, 1.0f));
    
    /* Normalisation finale dans [0.0f, 1.0f] pour calculer l'angle en radians */
    float angle_normalise = (noise_angle + 1.0f) * 0.5f;
    float angle = angle_normalise * 2.0f * (float)M_PI;

    /* =========================================================================
     * OCTAVE 2 : VITESSE DU VENT (FBM Fractale)
     * ========================================================================= */
    /* Rotation asymétrique de ~30 degrés pour empêcher le parallélisme visuel 
     * CORRECTION : Remplacement du 'rz' erroné par 'fz' à droite du signe égal */
    float rx = (fx * 0.866025f) - (fz * 0.500000f);
    float rz = (fx * 0.500000f) + (fz * 0.866025f);

    float nx_speed = rx - 143.619f;
    float nz_speed = rz + 87.111f;

    float noise_speed = zyn_fractal_noise2d(nx_speed, nz_speed, octaves, persistence, lacunarity);
    
    /* Amplification et clamping strict de la vitesse brute dans [-1.0f, 1.0f] */
    noise_speed *= fbm_booster;
    noise_speed = fmaxf(-1.0f, fminf(noise_speed, 1.0f));
    
    /* Normalisation linéaire finale [0.0f, 1.0f] */
    float speed_normalisee = (noise_speed + 1.0f) * 0.5f;
    
    /* Vitesse maximale fixée à 90 km/h pour l'échelle macro */
    float speed = speed_normalisee * 90.0f; 

    /* Conversion Polaire -> Cartésienne */
    WindVector global_wind;
    global_wind.dx = speed * cosf(angle);
    global_wind.dy = speed * sinf(angle);

    return global_wind;
}
