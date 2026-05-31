/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 30/05/2026
 *
 * wind_global  :
 * utilisation :
 *
 * =============================================================================*/
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// En-têtes requis sous Linux/POSIX pour manipuler 'struct stat' et 'mkdir'
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <zynthar.h>
#include "wind_global.h"
#include "zyn_noise.h"

WindVector get_global_wind(int32_t chunk_x, int32_t chunk_y) {
    // Échelle basse fréquence (changement à l'échelle des grands climats du monde)
    float scale = 0.02f; 
    float fx = (float)chunk_x * scale;
    float fz = (float)chunk_y * scale; // On passe l'axe Y du chunk sur le paramètre Z de l'algo

    // Octave 1 : Direction du vent
    // On assume que zyn_noise2d renvoie une valeur dans [0.0, 1.0] ou [-1.0, 1.0]
    // Si ton bruit est signé [-1, 1], on le ramène entre 0 et 1 pour l'angle si nécessaire.
    float noise_angle = zyn_noise2d(fx, fz);
    
    // Protection/Normalisation au cas où ton bruit est signé [-1.0, 1.0]
    if (noise_angle < 0.0f) noise_angle = (noise_angle + 1.0f) * 0.5f;
    
    float angle = noise_angle * 2.0f * (float)M_PI;

    // Octave 2 : Vitesse du vent
    // On applique un décalage (offset) sur les coordonnées pour découpler la vitesse de la direction
    float noise_speed = zyn_noise2d(fx + 50.0f, fz + 50.0f);
    if (noise_speed < 0.0f) noise_speed = (noise_speed + 1.0f) * 0.5f;
    
    // Vitesse maximale fixée à 90 km/h pour l'échelle macro
    float speed = noise_speed * 90.0f; 

    // Conversion Polaire -> Cartésienne
    WindVector global_wind;
    global_wind.dx = speed * cosf(angle);
    global_wind.dy = speed * sinf(angle);

    return global_wind;
}
