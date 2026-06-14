#ifndef ZYN_MATH_H
#define ZYN_MATH_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_math.h
 * Date    : 14/06/2026
 * ============================================================================= */
#include <stdint.h>
/**
 * @brief Interpolation bilinéaire pure et déterministe en point fixe (décimètres).
 * * @param d    Côté du carré de référence (en dm)
 * @param h_nw Altitude coin Nord-Ouest (x=0, z=0)
 * @param h_ne Altitude coin Nord-Est  (x=d, z=0)
 * @param h_sw Altitude coin Sud-Ouest (x=0, z=d)
 * @param h_se Altitude coin Sud-Est  (x=d, z=d)
 * @param x    Coordonnée X du point cible (0 <= x <= d)
 * @param z    Coordonnée Z du point cible (0 <= z <= d)
 * @return int32_t Altitude interpolée au point (x, z) en dm
 */
int32_t zyn_math_bilinear_int(uint32_t d, 
                              int32_t h_nw, int32_t h_ne, 
                              int32_t h_sw, int32_t h_se, 
                              uint32_t x, uint32_t z);
                              
// zyn_math.h

typedef struct {
    // Les 5 hauteurs d'autorité du Macro-Chunk (en dm)
    int32_t nw, ne, sw, se,n,s,w,e,n_wx,e_wx,s_wx,w_wx,n_wz,e_wz,s_wz,w_wz;
    int32_t c,d;
    uint64_t d_sq;
    int32_t world_x, world_z;
    // Les 4 amplitudes d'arêtes fournies par Chronos (en dm)
    int32_t amp_n, amp_s, amp_w, amp_e;
} ZynQuadraticGrid;

ZynQuadraticGrid zyn_math_forge_fractal_grid(int32_t h_nw, int32_t h_ne, 
                                             int32_t h_sw, int32_t h_se, 
                                             int32_t h_centre,
                                             int32_t world_x, int32_t world_z,
                                             uint32_t d,
                                             int32_t amp_n, int32_t amp_s,
                                             int32_t amp_w, int32_t amp_e);
                                             
                                             /**
 * @brief Interpolation biquadratique 2D déterministe sur une grille 3x3 en entiers.
 * @param grid La grille 3x3 contenant les 9 points de contrôle (en dm).
 * @param x    Coordonnée X locale de la cible (0 <= x <= d).
 * @param z    Coordonnée Z locale de la cible (0 <= z <= d).
 * @return int32_t L'altitude interpolée résultante (en dm).
 */
int32_t zyn_math_biquadratic_int(ZynQuadraticGrid grid,uint32_t x, uint32_t z);

#endif // ZYN_MATH_H
