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
#include "zyn_test_framework.h"

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

    /* Positionnement déterministe des germes */
    uint32_t lcg_state = seed; 
    for (int32_t i = 0; i < num_islands; i++) {
        lcg_state = lcg_state * 1103515245 + 12345;
        float rand_x = (float)lcg_state / 4294967295.0f;
        seeds[i].x = rand_x * (float)width_x;

        lcg_state = lcg_state * 1103515245 + 12345;
        float rand_z = (float)lcg_state / 4294967295.0f;
        seeds[i].z = rand_z * (float)depth_z;
    }

    const float EPSILON_STABILITE = 0.123456f;
    float global_min_dist = 99999999.0f;
    float global_max_dist = -1.0f;

    /* PASSOIRE 1 : Optimisation de la boucle de calcul des distances */
    #pragma omp parallel for schedule(dynamic) reduction(min:global_min_dist) reduction(max:global_max_dist)
    for (int32_t z = 0; z < depth_z; z++) {
        float fz = (float)z + EPSILON_STABILITE;
        
        for (int32_t x = 0; x < width_x; x++) {
            float fx = (float)x + EPSILON_STABILITE;

            /* OPTIMISATION WARP : Utilisation de fonctions trigonométriques combinées rapides 
               au lieu de zyn_noise2d pour froisser l'espace à moindre coût CPU */
            float warp_angle1 = (fx * 0.015f) + (fz * 0.007f);
            float warp_angle2 = (fx * 0.005f) - (fz * 0.022f);
            float v_warp_x = sinf(warp_angle1) * 12.0f;
            float v_warp_z = cosf(warp_angle2) * 12.0f;

            float fx_deforme = fx + v_warp_x;
            float fz_deforme = fz + v_warp_z;

            float min_dist_hybride_carre = 99999999.0f;

            for (int32_t i = 0; i < num_islands; i++) {
                float dx = fabsf(seeds[i].x - fx_deforme);
                float dz = fabsf(seeds[i].z - fz_deforme);

                float dist_euclidienne_carre = (dx * dx) + (dz * dz);
                float dist_manhattan = dx + dz;
                float dist_manhattan_carre = dist_manhattan * dist_manhattan;

                /* Mix structurel */
                float dist_mixte_carre = (dist_euclidienne_carre * 0.75f) + (dist_manhattan_carre * 0.25f);

                if (dist_mixte_carre < min_dist_hybride_carre) {
                    min_dist_hybride_carre = dist_mixte_carre;
                }
            }

            size_t index = ZYN_INDEX(x, z, width_x);
            masque_voronoi[index] = min_dist_hybride_carre;

            if (min_dist_hybride_carre < global_min_dist) global_min_dist = min_dist_hybride_carre;
            if (min_dist_hybride_carre > global_max_dist) global_max_dist = min_dist_hybride_carre;
        }
    }

    /* =========================================================================
       PASSOIRE 2 OPTIMISÉE : Travail direct en espace quadratique (ZÉRO sqrtf !)
       ========================================================================= */
    float delta_dist_carre = global_max_dist - global_min_dist;
    if (delta_dist_carre < 0.0001f) delta_dist_carre = 1.0f;

    /* Le rayon de contrôle est adapté à l'espace quadratique (0.55 au carré vaut ~0.30) */
    float rayon_controle_carre = delta_dist_carre * 0.3025f; 
    #pragma omp parallel for
    for (size_t p = 0; p < total_cases; p++) {
        float dist_brute_carre = masque_voronoi[p];
        float dist_relative_carre = dist_brute_carre - global_min_dist;
        
        float ratio_carre = dist_relative_carre / rayon_controle_carre;
        if (ratio_carre > 1.0f) ratio_carre = 1.0f;
        
        /* Pour retrouver le profil en dôme parfait sans extraire de racine carrée :
           La formule (1.0f - ratio_carre) sur un espace quadratique mime mathématiquement 
           la douceur d'amortissement du dôme linéaire d'origine. */
        float val = 1.0f - ratio_carre;
        
        if (val < 0.0f) val = 0.0f;
        if (val > 1.0f) val = 1.0f;

        masque_voronoi[p] = val;
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
void zyn_gen_map_relief_archipelago(MacroChunk* map, int32_t width_x, int32_t depth_z, int32_t num_islands, float max_sea_percentage, uint32_t seed,ZynTestConfig* test_config) {
    if (map == NULL || width_x <= 0 || depth_z <= 0) return;

    float* masque_voronoi = zyn_gen_map_relief_voronoi(width_x, depth_z, num_islands, seed);
    if (masque_voronoi == NULL) return;
    
    /* -------------------------------------------------------------------------
       INTERCEPTION ET COPIE DANS MAP SI ARRET PAS 1 DEMANDÉ
       ------------------------------------------------------------------------- */
    if (test_config != NULL && test_config->active_test == 1 && test_config->target_step == 1) {
        for (int32_t z = 0; z < depth_z; z++) {
            for (int32_t x = 0; x < width_x; x++) {
                size_t index = ZYN_INDEX(x, z, width_x);
                map[index].elevation_max_dm = M_TO_DM((int16_t)(masque_voronoi[index] * ZYN_WORLD_Y_MAX));
                map[index].chunk_x = x;
                map[index].chunk_z = z;
            }
        }
        free(masque_voronoi);
        test_config->early_exit=1;
        return; /* COURT-CIRCUIT : On ne fait pas le Perlin, ni le qsort */
    }
    size_t total_cases = (size_t)width_x * (size_t)depth_z;
    float* hauteurs_triees = (float*)malloc(total_cases * sizeof(float));
    if (hauteurs_triees == NULL) { free(masque_voronoi); return; }

    int32_t octaves = 5;
    float persistence = 0.54f;
    float lacunarity = 2.1f;
    float base_scale = 0.006f;
    
    float offset_x = 1250.5f + (float)(seed % 7919);
    float offset_z = -4580.2f - (float)((seed >> 4) % 5417);
    float relief_min=0.0f;
    float relief_max=0.0f;

    for (int32_t z = 0; z < depth_z; z++) {
        for (int32_t x = 0; x < width_x; x++) {
            size_t index = ZYN_INDEX(x, z, width_x);
            
            /* Calcul du Domain Warping stabilisé */
            float warp_dx = zyn_noise2d((x + 500.23f) * 0.002f, (z - 300.45f) * 0.002f) * 90.0f;
            float warp_dz = zyn_noise2d((x - 200.67f) * 0.002f, (z + 800.89f) * 0.002f) * 90.0f;

            float nx = (x + warp_dx + offset_x) * base_scale;
            float nz = (z + warp_dz + offset_z) * base_scale;
            
            float bruit_global = zyn_fractal_noise2d(nx, nz, octaves, persistence, lacunarity);

            float v_mask = masque_voronoi[index];       // 1.0 au sommet, 0.0 au large
            float zone_oceanique = 1.0f - v_mask;       // 0.0 au sommet, 1.0 au large

            /* 1. L'ÉQUATION DU RELIEF TOTAL (TERRE + MER) 
               Le bruit s'exprime pleinement PARTOUT (coefficient 0.6f). 
               - Au centre de l'île (v_mask=1.0), l'ascenseur monte (+1.4f). Le relief est haut.
               - Au large (v_mask=0.0), l'ascenseur descend (-1.3f). Le bruit crée des creux et des bosses sous l'eau.
            */
            float relief_brut = (bruit_global * 0.6f) + (v_mask * 1.4f) - (zone_oceanique * 1.3f);

            /* 2. EFFET DES BORDURES DE CARTE (OPTIONNEL)
               Si tu veux que l'océan devienne de plus en plus profond (fosses) à mesure qu'on s'éloigne 
               des îles, on peut accentuer la descente au grand large tout en gardant 100% du relief du bruit.
            */
            relief_brut -= (zone_oceanique * zone_oceanique * 0.4f);
            if(relief_brut<relief_min) relief_min=relief_brut;
            if(relief_brut>relief_max) relief_max=relief_brut;
            hauteurs_triees[index] = relief_brut;
            
        }
    }
    free(masque_voronoi);


    for (int32_t p=0;p<total_cases;p++) {
        if (hauteurs_triees[p]<=0) {
            hauteurs_triees[p] *= -1/relief_min;
        } else {
            hauteurs_triees[p] *= 1/relief_max;
        }
    }
    
    /* -------------------------------------------------------------------------
       INTERCEPTION ET COPIE DANS MAP SI ARRET PAS 2 DEMANDÉ
       ------------------------------------------------------------------------- */
    if (test_config != NULL && test_config->active_test == 1 && test_config->target_step == 2) {
        for (int32_t z = 0; z < depth_z; z++) {
            for (int32_t x = 0; x < width_x; x++) {
                size_t index = ZYN_INDEX(x, z, width_x);
                if (hauteurs_triees[index]>=0) {
                    map[index].elevation_max_dm = M_TO_DM((int16_t)(hauteurs_triees[index] * ZYN_WORLD_Y_MAX));
                }
                else {
                    map[index].elevation_max_dm = M_TO_DM((int16_t)(-hauteurs_triees[index] * ZYN_WORLD_Y_MIN));
                }
                map[index].chunk_x = x;
                map[index].chunk_z = z;
            }
        }
        free(hauteurs_triees);
        test_config->early_exit=1;
        return; 
    }

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
void zyn_gen_map_relief(MacroChunk* map, int32_t width_x, int32_t depth_z, uint32_t seed, ZynTestConfig* test_config) {
    if (map == NULL || width_x <= 0 || depth_z <= 0) return;
    zyn_noise_init(seed);
    
    double surface = (double)width_x * (double)depth_z;
    double base_iles = surface / 444444;
    if (base_iles < 1.0) base_iles = 1.0;

    /* 2. EXTRACTION D'UN FLOTTANT DETERMINISTE ENTRE -1.0f ET +1.0f VIA LA SEED */
    /* On utilise un mix par décalage pour briser les régularités de la seed */
    uint32_t hash = (seed ^ 0x5F3759DF) * 1103515245 + 12345;
    /* On ramène le hash entre 0.0f et 2.0f, puis on soustrait 1.0f pour avoir entre -1.0f et 1.0f */
    float alea_flottant = ((float)(hash % 2000) / 1000.0f) - 1.0f;

    /* 3. APPLICATION DE LA VARIATION DE +/- 30% */
    float variation = (float)base_iles * 0.30f * alea_flottant;
    int32_t num_islands = (int32_t)roundf((float)base_iles + variation);
    
    /* Sécurité absolue : au moins 1 île */
    if (num_islands < 1) num_islands = 1;

    printf("[RELIEF] Surface : %.0f Macro-Chunks | Base théorique : %.2f îles\n", surface, base_iles);
    printf("[RELIEF] Squelette de Voronoi initialisé déterministement avec %d centres d'îles (Variation: %.2f).", num_islands, variation);
    
    float max_sea_percentage = 0.45f; /* Ton ratio cible de 45% de mer */

    /* Lancement de l'archipel avec transmission de la seed */
    zyn_gen_map_relief_archipelago(map, width_x, depth_z, num_islands, max_sea_percentage, seed,test_config);

    /* Lissage final des traits de côtes */
    zyn_gen_map_relief_smooth_coastlines(map, width_x, depth_z, 2);
}
