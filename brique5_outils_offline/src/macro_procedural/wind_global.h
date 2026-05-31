#ifndef WIND_GLOBAL_H
#define WIND_GLOBAL_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : wind_global.h
 * Date    : 30/05/2026
 * ============================================================================= */

#include <stdint.h>

typedef struct {
    float dx; // Composante de vitesse sur X
    float dy; // Composante de vitesse sur Y
} WindVector;

// Calcule le vent global pur pour un macro-chunk donné (Fonction pure)
WindVector get_global_wind(int32_t chunk_x, int32_t chunk_y);

#endif // WIND_GLOBAL_H
