/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 14/06/2026
 *
 * zyn_math  :
 * utilisation :
 *
 * =============================================================================*/
  
/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_math.c
 * ============================================================================= */

#include "zyn_math.h"

int32_t zyn_math_bilinear_int(uint32_t d, 
                              int32_t h_nw, int32_t h_ne, 
                              int32_t h_sw, int32_t h_se, 
                              uint32_t x, uint32_t z) 
{
    // 1. Calcul des distances complémentaires (poids horizontaux et verticaux inverse)
    uint64_t x_inv = (uint64_t)(d - x);
    uint64_t z_inv = (uint64_t)(d - z);
    
    uint64_t x64 = (uint64_t)x;
    uint64_t z64 = (uint64_t)z;

    // 2. Calcul des surfaces (poids d'influence) pour chacun des 4 coins
    // Le typage en uint64_t garantit que le produit ne débordera pas.
    uint64_t w_nw = x_inv * z_inv;
    uint64_t w_ne = x64   * z_inv;
    uint64_t w_sw = x_inv * z64;
    uint64_t w_se = x64   * z64;

    // 3. Accumulation pondérée des altitudes en 64 bits signés
    int64_t accumulator = ((int64_t)w_nw * h_nw) + 
                          ((int64_t)w_ne * h_ne) + 
                          ((int64_t)w_sw * h_sw) + 
                          ((int64_t)w_se * h_se);

    // 4. Division par la surface totale du carré (d * d)
    // On effectue la multiplication au dénominateur en 64 bits.
    uint64_t total_surface = (uint64_t)d * (uint64_t)d;

    // Arrondi arithmétique pour minimiser l'erreur de la division entière (optionnel mais recommandé)
    // On ajoute la moitié du diviseur avant de diviser pour obtenir un arrondi au plus proche.
    if (accumulator >= 0) {
        accumulator = (accumulator + (int64_t)(total_surface / 2)) / (int64_t)total_surface;
    } else {
        accumulator = (accumulator - (int64_t)(total_surface / 2)) / (int64_t)total_surface;
    }

    // 5. Restitution de l'altitude sous forme d'entier 32 bits
    return (int32_t)accumulator;
}

/**
 * @brief Fonction de hachage 2D ultra-rapide et déterministe pour entiers.
 * Renvoie une valeur pseudo-aléatoire entre -max_amplitude et +max_amplitude (en dm).
 */
static inline int32_t zyn_math_hash_2d(int32_t x, int32_t z, int32_t max_amplitude) {
    if (max_amplitude == 0) return 0;
    
    // Algorithme de hachage par permutation de bits (déterministe par ALU)
    uint32_t hash = (uint32_t)x * 73856093U ^ (uint32_t)z * 19349663U;
    hash = (hash ^ (hash >> 16)) * 2147483647U;
    hash = hash ^ (hash >> 15);
    
    // Ramener la valeur dans la plage [-max_amplitude, +max_amplitude]
    int32_t range = max_amplitude * 2;
    int32_t offset = (int32_t)(hash % (uint32_t)range);
    
    return offset - max_amplitude;
}

ZynQuadraticGrid zyn_math_forge_fractal_grid(int32_t h_nw, int32_t h_ne, 
                                             int32_t h_sw, int32_t h_se, 
                                             int32_t h_centre,
                                             int32_t world_x, int32_t world_z,
                                             uint32_t d,
                                             int32_t amp_n, int32_t amp_s,
                                             int32_t amp_w, int32_t amp_e) 
{
    ZynQuadraticGrid grid;
    
    // 1. Assignation des autorités immuables (les 4 coins et le centre)
    grid.nw = h_nw; 
    grid.ne = h_ne; 
    grid.sw = h_sw; 
    grid.se = h_se;
    grid.c  = h_centre;
    grid.world_x = world_x;
    grid.world_z = world_z;
    grid.d=d;
    grid.d_sq=d*d;

    // 2. Calcul des positions mondiales exactes des 4 milieux d'arêtes
    int32_t half_d = (int32_t)(d / 2);
    
    grid.n_wx = world_x + half_d;  grid.n_wz = world_z;
    grid.s_wx = world_x + half_d;  grid.s_wz = world_z + (int32_t)d;
    grid.w_wx = world_x;           grid.w_wz = world_z + half_d;
    grid.e_wx = world_x + (int32_t)d;  grid.e_wz = world_z + half_d;

    // 3. Interpolation linéaire de base + injection du bruit contrôlé par l'amplitude de l'arête
    grid.n = ((h_nw + h_ne + 1) >> 1) + zyn_math_hash_2d(grid.n_wx, grid.n_wz, amp_n);
    grid.s = ((h_sw + h_se + 1) >> 1) + zyn_math_hash_2d(grid.s_wx, grid.s_wz, amp_s);
    grid.w = ((h_nw + h_sw + 1) >> 1) + zyn_math_hash_2d(grid.w_wx, grid.w_wz, amp_w);
    grid.e = ((h_ne + h_se + 1) >> 1) + zyn_math_hash_2d(grid.e_wx, grid.e_wz, amp_e);

    return grid;
}


__attribute__((hot))
int32_t zyn_math_biquadratic_int(ZynQuadraticGrid grid, uint32_t x, uint32_t z) 
{
    const uint64_t d64 = grid.d;
    const uint64_t x64 = x;
    const uint64_t z64 = z;
    const uint64_t d_sq = grid.d_sq;
    const int64_t dmx64=d64 - x64;
    const int64_t dmz64=d64 - z64;

    const int64_t w_x0 = (int64_t)((dmx64) * (dmx64 - x64)); 
    const int64_t w_x1 = (int64_t)(4 * x64 * (dmx64));
    const int64_t w_x2 = (int64_t)(x64 * (x64 - dmx64));

    const int64_t h_n_scaled = (grid.nw * w_x0) + (grid.n * w_x1) + (grid.ne * w_x2);
    const int64_t h_c_scaled = (grid.w  * w_x0) + (grid.c * w_x1) + (grid.e  * w_x2);
    const int64_t h_s_scaled = (grid.sw * w_x0) + (grid.s * w_x1) + (grid.se * w_x2);

    // Poids pour la coordonnée Z
    const int64_t w_z0 = (int64_t)((dmz64) * (dmz64 - z64));
    const int64_t w_z1 = (int64_t)(4 * z64 * (dmz64));
    const int64_t w_z2 = (int64_t)(z64 * (z64 - dmz64));

    // Passe verticale finale combinée (Échelle finale = d_sq * d_sq)
    int64_t final_scaled = (h_n_scaled * w_z0) + (h_c_scaled * w_z1) + (h_s_scaled * w_z2);

    const uint64_t total_surface_4d = d_sq * d_sq; 
    const int64_t half_surface_4d = (int64_t)(total_surface_4d >> 1);

    int64_t is_positive = (final_scaled >= 0);
    int64_t offset = (is_positive * half_surface_4d) - ((1 - is_positive) * half_surface_4d);

    return (int32_t)((final_scaled + offset) / (int64_t)total_surface_4d);
}
