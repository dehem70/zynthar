//////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                      //
//                                          ZYNTHAR v0.1                                                //
//                                                                                                      //
// Auteur : Dehem70                                                                                     //
// Date   : 28/05/2026                                                                                  //
//                                                                                                      //
// zyn_noise   ; INTERFACE DU MOTEUR MATHÉMATIQUE DE BRUIT PROCÉDURAL (2D & 3D)                         //
//                                                                                                      //
//////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zyn_noise.h"
#include <math.h>

/* Table de permutation interne (taille 512 pour éviter les modulos dans les index imbriqués) */
static uint8_t p[512];

/* =============================================================================
 * UTILITAIRES INTERNES DE GRADIENT
 * ============================================================================= */

/**
 * @brief Calcule le produit scalaire entre un gradient pseudo-aléatoire 2D et la distance.
 * (Version 4 directions issue de ta maquette Python)
 */
static inline float zyn_grad2d(uint8_t hash, float x, float y) {
    switch (hash & 3) {
        case 0:  return  x + y;
        case 1:  return -x + y;
        case 2:  return  x - y;
        case 3:  return -x - y;
        default: return 0.0f; /* Évite les avertissements du compilateur */
    }
}

/**
 * @brief Calcule le produit scalaire pour le bruit 3D.
 * Utilise les 12 vecteurs gradients pointant vers les arêtes d'un cube pour éviter les artefacts.
 */
static inline float zyn_grad3d(uint8_t hash, float x, float y, float z) {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

/* =============================================================================
 * ENTRÉE DE L'INTERFACE : INITIALISATION DE LA SEED
 * ============================================================================= */

void zyn_noise_init(uint32_t seed_value) {
    /* Tableau temporaire pour le mélange de base (0 à 255) */
    uint8_t base_p[256];
    for (int32_t i = 0; i < 256; i++) {
        base_p[i] = (uint8_t)i;
    }

    /* Générateur pseudo-aléatoire déterministe (LCG) pour mélanger la table */
    uint32_t next_random = seed_value;
    for (int32_t i = 255; i > 0; i--) {
        next_random = next_random * 1103515245 + 12345;
        int32_t target_index = (int32_t)((next_random / 65536) % (i + 1));
        
        /* Permutation (Shuffle) */
        uint8_t temp = base_p[i];
        base_p[i] = base_p[target_index];
        base_p[target_index] = temp;
    }

    /* Duplication de la table sur les 512 entrées pour sécuriser les accès indexés */
    for (int32_t i = 0; i < 512; i++) {
        p[i] = base_p[i & 255];
    }
}

/* =============================================================================
 * GENERATEURS DE BRUIT DE BASE (2D & 3D)
 * ============================================================================= */

float zyn_noise2d(float x, float y) {
    /* Détermination des coordonnées de la cellule virtuelle encapsulante */
    int32_t X = ((int32_t)floorf(x)) & 255;
    int32_t Y = ((int32_t)floorf(y)) & 255;

    /* Calcul des coordonnées relatives/fractionnaires dans la cellule */
    float xf = x - floorf(x);
    float yf = y - floorf(y);

    /* Application du lissage de Perlin (courbe en S) */
    float u = zyn_fade(xf);
    float v = zyn_fade(yf);

    /* Récupération des signatures de hachage des 4 coins du carré */
    uint8_t aa = p[p[X] + Y];
    uint8_t ab = p[p[X] + Y + 1];
    uint8_t ba = p[p[X + 1] + Y];
    uint8_t bb = p[p[X + 1] + Y + 1];

    /* Interpolations successives des produits scalaires des gradients */
    float res = zyn_lerp(
        zyn_lerp(zyn_grad2d(aa, xf, yf),         zyn_grad2d(ba, xf - 1.0f, yf), u),
        zyn_lerp(zyn_grad2d(ab, xf, yf - 1.0f),  zyn_grad2d(bb, xf - 1.0f, yf - 1.0f), u),
        v
    );

    return res;
}

float zyn_noise3d(float x, float y, float z) {
    /* Cellule virtuelle 3D encapsulante */
    int32_t X = ((int32_t)floorf(x)) & 255;
    int32_t Y = ((int32_t)floorf(y)) & 255;
    int32_t Z = ((int32_t)floorf(z)) & 255;

    /* Coordonnées fractionnaires dans le cube */
    float xf = x - floorf(x);
    float yf = y - floorf(y);
    float zf = z - floorf(z);

    /* Facteurs de lissage */
    float u = zyn_fade(xf);
    float v = zyn_fade(yf);
    float w = zyn_fade(zf);

    /* Hachage des 8 sommets du cube */
    int32_t A  = p[X] + Y;
    uint8_t aa = p[A] + Z;
    uint8_t ab = p[A + 1] + Z;
    int32_t B  = p[X + 1] + Y;
    uint8_t ba = p[B] + Z;
    uint8_t bb = p[B + 1] + Z;

    /* Triple interpolation linéaire (Trilinear interpolation) */
    return zyn_lerp(
        zyn_lerp(
            zyn_lerp(zyn_grad3d(p[aa], xf, yf, zf),          zyn_grad3d(p[ba], xf - 1.0f, yf, zf), u),
            zyn_lerp(zyn_grad3d(p[ab], xf, yf - 1.0f, zf),   zyn_grad3d(p[bb], xf - 1.0f, yf - 1.0f, zf), u),
            v
        ),
        zyn_lerp(
            zyn_lerp(zyn_grad3d(p[aa + 1], xf, yf, zf - 1.0f),         zyn_grad3d(p[ba + 1], xf - 1.0f, yf, zf - 1.0f), u),
            zyn_lerp(zyn_grad3d(p[ab + 1], xf, yf - 1.0f, zf - 1.0f),  zyn_grad3d(p[bb + 1], xf - 1.0f, yf - 1.0f, zf - 1.0f), u),
            v
        ),
        w
    );
}

/* =============================================================================
 * ALGORITHMES FRACTALS (FBM) SUPERPOSANT LES OCTAVES
 * ============================================================================= */

float zyn_fractal_noise2d(float x, float y, int32_t octaves, float persistence, float lacunarity) {
    float total = 0.0f;
    float amplitude = 1.0f;
    float frequence = 1.0f;
    float max_amplitude = 0.0f;

    for (int32_t i = 0; i < octaves; i++) {
        total += zyn_noise2d(x * frequence, y * frequence) * amplitude;
        max_amplitude += amplitude;
        amplitude *= persistence;
        frequence *= lacunarity;
    }

    return total / max_amplitude;
}

float zyn_fractal_noise3d(float x, float y, float z, int32_t octaves, float persistence, float lacunarity) {
    float total = 0.0f;
    float amplitude = 1.0f;
    float frequence = 1.0f;
    float max_amplitude = 0.0f;

    for (int32_t i = 0; i < octaves; i++) {
        total += zyn_noise3d(x * frequence, y * frequence, z * frequence) * amplitude;
        max_amplitude += amplitude;
        amplitude *= persistence;
        frequence *= lacunarity;
    }

    return total / max_amplitude;
}
