/* =============================================================================
 *
 * ZYNTHAR v0.1
 *
 * Auteur : Dehem70
 * Date   : 28/05/2026
 *
 * zyn_gen_map_relief : Génération procédurale du relief de la carte macro
 * Intègre les masques de Voronoi, le bruit fractal et un automate cellulaire.
 * Aligné sur l'axe horizontal longitudinal Z et le stockage packagé (décimètres).
 *
 * =============================================================================*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

#include <zynthar.h>
#include "zyn_noise.h"
#include "zyn_gen_map_relief.h"

typedef struct {
    float x;
    float z;
} IslandSeed;

MacroChunk* zyn_gen_map_relief_alloc(int32_t width_x, int32_t depth_z) {
    if (width_x <= 0 || depth_z <= 0) return NULL;
    size_t total_chunks = (size_t)width_x * (size_t)depth_z;
    return (MacroChunk*)calloc(total_chunks, sizeof(MacroChunk));
}

void zyn_gen_map_relief_free(MacroChunk* map) {
    if (map != NULL) free(map);
}

/* =============================================================================
 * GENERATION DU MASQUE DE VORONOI ULTRA-SECURISE
 * ============================================================================= */
float* zyn_gen_map_relief_voronoi(int32_t width_x, int32_t depth_z, int32_t num_islands, uint32_t seed) {
    size_t total_cases = (size_t)width_x * (size_t)depth_z;
    float* masque_voronoi = (float*)malloc(total_cases * sizeof(float));
    if (masque_voronoi == NULL) return NULL;
    
    IslandSeed* seeds = (IslandSeed*)malloc(sizeof(IslandSeed) * num_islands);
    if (seeds == NULL) {
        free(masque_voronoi);
        return NULL;
    }

    uint32_t lcg_state = seed; 
    for (int32_t i = 0; i < num_islands; i++) {
        lcg_state = lcg_state * 1103515245 + 12345;
        float rand_x = (float)lcg_state / 4294967295.0f;
        seeds[i].x = rand_x * (float)width_x;

        lcg_state = lcg_state * 1103515245 + 12345;
        float rand_z = (float)lcg_state / 4294967295.0f;
        seeds[i].z = rand_z * (float)depth_z;
    }

    float max_dist_normale = (width_x < depth_z ? (float)width_x : (float)depth_z) / 3.5f; 
    float max_dist_carre = max_dist_normale * max_dist_normale;

    /* RECONDUCTION DE L'EPSILON DE STABILISATION GEOMETRIQUE */
    const float EPSILON_STABILITE = 0.123456f;

    for (int32_t z = 0; z < depth_z; z++) {
        float fz = (float)z + EPSILON_STABILITE; // Sécurisé

        for (int32_t x = 0; x < width_x; x++) {
            float fx = (float)x + EPSILON_STABILITE; // Sécurisé
            float min_dist_carre = 99999999.0f;

            for (int32_t i = 0; i < num_islands; i++) {
                float dx = seeds[i].x - fx;
                float dz = seeds[i].z - fz;
                float dist_carre = (dx * dx) + (dz * dz);
                if (dist_carre < min_dist_carre) min_dist_carre = dist_carre;
            }

            float val = 1.0f - (min_dist_carre / max_dist_carre);
            if (val < 0.0f) val = 0.0f;

            size_t index = ZYN_INDEX(x, z, width_x);
            masque_voronoi[index] = val;
        }
    }

    free(seeds);
    return masque_voronoi;
}
static int comparer_floats(const void* a, const void* b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    if (fa < fb) return -1;
    if (fa > fb) return 1;
    return 0;
}

/* =============================================================================
 * FUSION DU RELIEF ET SCULPTURE DES CONTINENTS
 * ============================================================================= */
void zyn_gen_map_relief_archipelago(MacroChunk* map, int32_t width_x, int32_t depth_z, int32_t num_islands, float max_sea_percentage, uint32_t seed) {
    if (map == NULL || width_x <= 0 || depth_z <= 0) return;

    float* masque_voronoi = zyn_gen_map_relief_voronoi(width_x, depth_z, num_islands, seed);
    if (masque_voronoi == NULL) return;

    size_t total_cases = (size_t)width_x * (size_t)depth_z;
    float* hauteurs_triees = (float*)malloc(total_cases * sizeof(float));
    if (hauteurs_triees == NULL) { free(masque_voronoi); return; }

    int32_t octaves = 5;
    float persistence = 0.54f;
    float lacunarity = 2.1f;
    float base_scale = 0.006f;
    
    float offset_x = 1250.5f + (float)(seed % 7919);
    float offset_z = -4580.2f - (float)((seed >> 4) % 5417);

    /* EPSILON ANTI-FRACTURE : On introduit un bruit de décalage constant 
       pour éloigner définitivement les float des zéros d'arrondis des registres */
    const float EPSILON_STABILITE = 0.123456f;

    for (int32_t z = 0; z < depth_z; z++) {
        /* On applique l'epsilon sur l'axe Z */
        float fz = (float)z + EPSILON_STABILITE;

        for (int32_t x = 0; x < width_x; x++) {
            size_t index = ZYN_INDEX(x, z, width_x);
            
            /* On applique l'epsilon sur l'axe X */
            float fx = (float)x + EPSILON_STABILITE;

            /* Calcul du Domain Warping stabilisé */
            float warp_dx = zyn_noise2d((fx + 500.23f) * 0.002f, (fz - 300.45f) * 0.002f) * 40.0f;
            float warp_dz = zyn_noise2d((fx - 200.67f) * 0.002f, (fz + 800.89f) * 0.002f) * 40.0f;

            float nx = (fx + warp_dx + offset_x) * base_scale;
            float nz = (fz + warp_dz + offset_z) * base_scale;
            
            float bruit_global = zyn_fractal_noise2d(nx, nz, octaves, persistence, lacunarity);

            float v_mask = masque_voronoi[index];
            float zone_oceanique = 1.0f - v_mask;

            float relief_brut = (bruit_global * 0.5f) + (v_mask * 1.4f) - (zone_oceanique * 0.8f);
            hauteurs_triees[index] = relief_brut;
        }
    }

    free(masque_voronoi);

    /* Code de tri classique remis au propre */
    float* copie_pour_tri = (float*)malloc(total_cases * sizeof(float));
    if (copie_pour_tri == NULL) { free(hauteurs_triees); return; }
    memcpy(copie_pour_tri, hauteurs_triees, total_cases * sizeof(float));
    qsort(copie_pour_tri, total_cases, sizeof(float), comparer_floats);

    size_t index_mer = (size_t)((double)total_cases * (double)max_sea_percentage);
    if (index_mer >= total_cases) index_mer = total_cases - 1;
    float niveau_mer_calcule = copie_pour_tri[index_mer];
    free(copie_pour_tri);

    float max_brut_terre = 0.0001f; 
    float min_brut_mer   = -0.0001f;

    for (size_t i = 0; i < total_cases; i++) {
        float alt_relative = hauteurs_triees[i] - niveau_mer_calcule;
        if (alt_relative > 0.0f && alt_relative > max_brut_terre) max_brut_terre = alt_relative;
        if (alt_relative < 0.0f && alt_relative < min_brut_mer)   min_brut_mer = alt_relative;
    }
    
    float max_monde_config_m  = (float)ZYN_WORLD_Y_MAX;
    float min_monde_config_m  = (float)ZYN_WORLD_Y_MIN;
    float coef_positif = max_monde_config_m / max_brut_terre;
    float coef_negatif = fabsf(min_monde_config_m) / fabsf(min_brut_mer);

    for (int32_t z = 0; z < depth_z; z++) {
        for (int32_t x = 0; x < width_x; x++) {
            size_t index = ZYN_INDEX(x, z, width_x);
            
            float alt_relative = hauteurs_triees[index] - niveau_mer_calcule;
            float alt_finale_m = 0.0f;

            if (alt_relative > 0.0f) {
                alt_finale_m = (alt_relative * coef_positif);
            } else {
                alt_finale_m = -(fabsf(alt_relative) * coef_negatif);
            }

            float alt_clamped_m = fmaxf(min_monde_config_m, fminf(alt_finale_m, max_monde_config_m));

            map[index].elevation_max_dm = (int16_t)roundf(alt_clamped_m * 10.0f);
            map[index].chunk_x = x;
            map[index].chunk_z = z;
        }
    }

    free(hauteurs_triees);
}
/* =============================================================================
 * LISSAGE DES CÔTES PAR AUTOMATE CELLULAIRE ELEVE AU TYPE SIZE_T
 * ============================================================================= */
void zyn_gen_map_relief_smooth_coastlines(MacroChunk* map, int32_t width_x, int32_t depth_z, int32_t iterations) {
    if (map == NULL || width_x <= 0 || depth_z <= 0 || iterations <= 0) return;

    size_t total_cases = (size_t)width_x * (size_t)depth_z;
    uint8_t* grille_binaire = (uint8_t*)malloc(total_cases * sizeof(uint8_t));
    if (grille_binaire == NULL) return;

   /* 1. Initialisation de la grille binaire avec la macro ZYN_INDEX */
    for (int32_t z = 0; z < depth_z; z++) {
        for (int32_t x = 0; x < width_x; x++) {
            size_t idx = ZYN_INDEX(x, z, width_x);
            grille_binaire[idx] = (map[idx].elevation_max_dm > 0) ? 1 : 0;
        }
    }

    uint8_t* nouvelle_grille = (uint8_t*)malloc(total_cases * sizeof(uint8_t));
    if (nouvelle_grille == NULL) {
        free(grille_binaire);
        return;
    }
    memcpy(nouvelle_grille, grille_binaire, total_cases);

    for (int32_t iter = 0; iter < iterations; iter++) {
        for (int32_t z = 1; z < depth_z - 1; z++) {
            size_t offset_ligne = (size_t)z * (size_t)width_x;
            size_t haut = offset_ligne - (size_t)width_x;
            size_t bas = offset_ligne + (size_t)width_x;

            for (int32_t x = 1; x < width_x - 1; x++) {
                size_t sx = (size_t)x;
                int32_t voisins_terre = 
                    grille_binaire[haut + sx - 1] + grille_binaire[haut + sx] + grille_binaire[haut + sx + 1] +
                    grille_binaire[offset_ligne + sx - 1]                     + grille_binaire[offset_ligne + sx + 1] +
                    grille_binaire[bas + sx - 1]  + grille_binaire[bas + sx]  + grille_binaire[bas + sx + 1];

                size_t index_actuel = offset_ligne + sx;
                
                if (grille_binaire[index_actuel] == 1 && voisins_terre < 4) {
                    nouvelle_grille[index_actuel] = 0; 
                } else if (grille_binaire[index_actuel] == 0 && voisins_terre >= 5) {
                    nouvelle_grille[index_actuel] = 1; 
                } else {
                    nouvelle_grille[index_actuel] = grille_binaire[index_actuel];
                }
            }
        }
        memcpy(grille_binaire, nouvelle_grille, total_cases);
    }

    /* 3. Réassignation finale des hauteurs ajustées dans la structure map */
    for (int32_t z = 0; z < depth_z; z++) {
        for (int32_t x = 0; x < width_x; x++) {
            size_t idx = ZYN_INDEX(x, z, width_x);
            int16_t alt_dm = map[idx].elevation_max_dm;

            if (grille_binaire[idx] == 1) {
                /* Si l'automate dit "Terre" mais que l'altitude était négative, on la redresse légèrement au-dessus de 0 */
                map[idx].elevation_max_dm = (alt_dm > 0) ? alt_dm : abs(alt_dm) + 1;
            } else {
                /* Si l'automate dit "Mer" mais que l'altitude était positive, on la fait couler sous le niveau 0 */
                map[idx].elevation_max_dm = (alt_dm < 0) ? alt_dm : -abs(alt_dm) - 1;
            }
        }
    }

    free(grille_binaire);
    free(nouvelle_grille);
}

/* =============================================================================
 * POINT D'ENTRÉE DU MODULE : NOMBRE D'ÎLES ET VARIATION TOTALE
 * ============================================================================= */
void zyn_gen_map_relief(MacroChunk* map, int32_t width_x, int32_t depth_z, uint32_t seed) {
    if (map == NULL || width_x <= 0 || depth_z <= 0) return;
    zyn_noise_init(seed);
    /* Nombre d'îles aléatoire et déterministe entre 5 et 15 basé sur la seed */
    int32_t num_islands = 1 + (int32_t)((seed ^ 0x5F3759DF) % 5);
    
    float max_sea_percentage = 0.45f; /* Ton ratio cible de 45% de mer */

    /* Lancement de l'archipel avec transmission de la seed */
    zyn_gen_map_relief_archipelago(map, width_x, depth_z, num_islands, max_sea_percentage, seed);

    /* Lissage final des traits de côtes */
    zyn_gen_map_relief_smooth_coastlines(map, width_x, depth_z, 2);
}
