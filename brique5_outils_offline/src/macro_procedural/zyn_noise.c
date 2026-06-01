/* =============================================================================
 *
 * ZYNTHAR v0.1
 *
 * Auteur : Dehem70
 * Date   : 28/05/2026
 *
 * zyn_noise : Moteur de génération mathématique de bruit procédural (2D & 3D)
 * Adapté aux axes 3D Babylon.js (X, Z: Horizontal, Y: Vertical)
 *
 * =============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include <zynthar.h>
#include "zyn_noise.h"

static uint8_t p[512];

/* =============================================================================
 * UTILITAIRES INTERNES DE GRADIENT
 * ============================================================================= */
static inline float zyn_grad2d(uint8_t hash, float x, float z) {
    float u = (hash & 1) ? -x : x;
    float v = (hash & 2) ? -z : z;
    return u + v;
}

static inline float zyn_grad3d(uint8_t hash, float x, float z, float y) {
    int h = hash & 15;
    float u = (h < 8) ? x : z;
    float v = (h < 4) ? z : ((h == 12 || h == 14) ? x : y);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

/* =============================================================================
 * INITIALISATION DE LA TABLE DE PERMUTATION
 * ============================================================================= */
void zyn_noise_init(uint32_t seed_value) {
    uint8_t base_p[256];
    for (int32_t i = 0; i < 256; i++) {
        base_p[i] = (uint8_t)i;
    }

    uint32_t next_random = seed_value;
    for (int32_t i = 255; i > 0; i--) {
        next_random = next_random * 1103515245 + 12345;
        int32_t target_index = (int32_t)((next_random / 65536) % (i + 1));
        
        uint8_t temp = base_p[i];
        base_p[i] = base_p[target_index];
        base_p[target_index] = temp;
    }

    for (int32_t i = 0; i < 512; i++) {
        p[i] = base_p[i & 255];
    }
}

/* =============================================================================
 * BRUIT 2D SÉCURISÉ (FIN DES LIGNES ARTEFACTS)
 * ============================================================================= */
float zyn_noise2d(float x, float z) {
    /* 1. floorf() gère nativement et correctement les coordonnées négatives */
    float fx = floorf(x);
    float fz = floorf(z);

    /* 2. Stockage dans des entiers signés 32 bits pour éviter le wrap-around à 255 */
    int32_t ix = (int32_t)fx;
    int32_t iz = (int32_t)fz;

    /* 3. Masquage binaire étanche & 255 pour rester dans les bornes du tableau p */
    int32_t X = ix & 255;
    int32_t Z = iz & 255;

    /* Coordonnées fractionnaires linéaires parfaites */
    float xf = x - fx;
    float zf = z - fz;

    float u = zyn_fade(xf);
    float v = zyn_fade(zf);

    /* Hachage sécurisé via le décalage propre de la table dupliquée */
    uint8_t aa = p[p[X] + Z];
    uint8_t ab = p[p[X] + Z + 1];
    uint8_t ba = p[p[X + 1] + Z];
    uint8_t bb = p[p[X + 1] + Z + 1];

    return zyn_lerp(
        zyn_lerp(zyn_grad2d(aa, xf, zf),         zyn_grad2d(ba, xf - 1.0f, zf), u),
        zyn_lerp(zyn_grad2d(ab, xf, zf - 1.0f),  zyn_grad2d(bb, xf - 1.0f, zf - 1.0f), u),
        v
    );
}

/* =============================================================================
 * BRUIT 3D SÉCURISÉ
 * ============================================================================= */
float zyn_noise3d(float x, float z, float y) {
    float fx = floorf(x);
    float fz = floorf(z);
    float fy = floorf(y);

    int32_t ix = (int32_t)fx;
    int32_t iz = (int32_t)fz;
    int32_t iy = (int32_t)fy;

    int32_t X = ix & 255;
    int32_t Z = iz & 255;
    int32_t Y = iy & 255;
    
    float xf = x - fx;
    float zf = z - fz;
    float yf = y - fy;

    float u = zyn_fade(xf);
    float v = zyn_fade(zf);
    float w = zyn_fade(yf);

    int32_t A  = p[X] + Z;
    uint8_t aa = p[A] + Y;
    uint8_t ab = p[A + 1] + Y;
    int32_t B  = p[X + 1] + Z;
    uint8_t ba = p[B] + Y;
    uint8_t bb = p[B + 1] + Y;

    return zyn_lerp(
        zyn_lerp(
            zyn_lerp(zyn_grad3d(p[aa], xf, zf, yf),          zyn_grad3d(p[ba], xf - 1.0f, zf, yf), u),
            zyn_lerp(zyn_grad3d(p[ab], xf, zf - 1.0f, yf),   zyn_grad3d(p[bb], xf - 1.0f, zf - 1.0f, yf), u),
            v
        ),
        zyn_lerp(
            zyn_lerp(zyn_grad3d(p[aa + 1], xf, zf, yf - 1.0f),          zyn_grad3d(p[ba + 1], xf - 1.0f, zf, yf - 1.0f), u),
            zyn_lerp(zyn_grad3d(p[ab + 1], xf, zf - 1.0f, yf - 1.0f),   zyn_grad3d(p[bb + 1], xf - 1.0f, zf - 1.0f, yf - 1.0f), u),
            v
        ),
        w
    );
}

/* =============================================================================
 * ALGORITHMES FRACTALS (FBM)
 * ============================================================================= */
float zyn_fractal_noise2d(float x, float z, int32_t octaves, float persistence, float lacunarity) {
    float total = 0.0f;
    float amplitude = 1.0f;
    float frequence = 1.0f;
    float max_amplitude = 0.0f;

    for (int32_t i = 0; i < octaves; i++) {
        total += zyn_noise2d(x * frequence, z * frequence) * amplitude;
        max_amplitude += amplitude;
        amplitude *= persistence;
        frequence *= lacunarity;
    }

    return total / max_amplitude;
}

float zyn_fractal_noise3d(float x, float z, float y, int32_t octaves, float persistence, float lacunarity) {
    float total = 0.0f;
    float amplitude = 1.0f;
    float frequence = 1.0f;
    float max_amplitude = 0.0f;

    for (int32_t i = 0; i < octaves; i++) {
        total += zyn_noise3d(x * frequence, z * frequence, y * frequence) * amplitude;
        max_amplitude += amplitude;
        amplitude *= persistence;
        frequence *= lacunarity;
    }

    return total / max_amplitude;
}
