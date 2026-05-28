#ifndef ZYN_NOISE_H
#define ZYN_NOISE_H

#include <stdint.h>

/* =============================================================================
 * INTERFACE DU MOTEUR MATHÉMATIQUE DE BRUIT PROCÉDURAL
 * ============================================================================= */

/**
 * @brief Initialise la table de permutation avec une graine (seed) spécifique.
 * @param seed_value Valeur de la graine. Si identique, la carte générée sera identique.
 */
void zyn_noise_init(uint32_t seed_value);

/**
 * @brief Génère une valeur de bruit de Perlin 2D classique pour des coordonnées continues.
 * @param x Coordonnée X continue (flottante).
 * @param y Coordonnée Y continue (flottante).
 * @return Valeur de bruit brute comprise entre -1.0 et 1.0.
 */
float zyn_noise2d(float x, float y);

/**
 * @brief Superpose plusieurs octaves de bruit de Perlin pour créer un effet fractal (FBM).
 * @param x Coordonnée X continue.
 * @param y Coordonnée Y continue.
 * @param octaves Nombre de couches de bruit superposées (plus d'octaves = plus de détails).
 * @param persistence Contrôle la perte d'amplitude à chaque octave (généralement 0.5).
 * @param lacunarity Contrôle le saut de fréquence à chaque octave (généralement 2.0).
 * @return Valeur de bruit fractale normalisée entre -1.0 et 1.0 (ou 0.0 et 1.0 selon traitement).
 */
float zyn_fractal_noise2d(float x, float y, int32_t octaves, float persistence, float lacunarity);

/**
 * @brief Génère une valeur de bruit de Perlin 3D classique pour des coordonnées continues.
 * @param x Coordonnée X continue.
 * @param y Coordonnée Y continue.
 * @param z Coordonnée Z continue (axe vertical/altitude).
 * @return Valeur de densité brute comprise entre -1.0 et 1.0.
 */
float zyn_noise3d(float x, float y, float z);

/**
 * @brief Superpose plusieurs octaves de bruit de Perlin 3D pour créer un effet fractal (FBM).
 * @param x Coordonnée X continue.
 * @param y Coordonnée Y continue.
 * @param z Coordonnée Z continue.
 * @param octaves Nombre de couches de bruit superposées.
 * @param persistence Contrôle la perte d'amplitude à chaque octave.
 * @param lacunarity Contrôle le saut de fréquence à chaque octave.
 * @return Valeur de densité fractale normalisée entre -1.0 et 1.0.
 */
float zyn_fractal_noise3d(float x, float y, float z, int32_t octaves, float persistence, float lacunarity);

/* =============================================================================
 * FONCTIONS MATHÉMATIQUES EN LIGNE (INLINE) POUR LES PERFORMANCES
 * ============================================================================= */

/**
 * @brief Fonction de lissage mathématique de Ken Perlin (6t^5 - 15t^4 + 10t^3).
 * Assure la continuité des dérivées secondes pour éviter les cassures visuelles.
 */
static inline float zyn_fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

/**
 * @brief Interpolation linéaire simple entre a et b selon le facteur t.
 */
static inline float zyn_lerp(float a, float b, float t) {
    return a + t * (b - a);
}

#endif /* ZYN_NOISE_H */
