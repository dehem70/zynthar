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
#include "zyn_utils.h"

typedef struct {
    float x;
    float z;
    float hauteur_max;    // Facteur d'altitude max associé à cette cellule (Idée 1 : 0.5 à 1.0)
    uint32_t id;
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
        seeds[i].id=i;
        lcg_state = lcg_state * 1103515245 + 12345;
        float rand_x = (float)lcg_state / 4294967295.0f;
        seeds[i].x = rand_x * (float)width_x;

        lcg_state = lcg_state * 1103515245 + 12345;
        float rand_z = (float)lcg_state / 4294967295.0f;
        seeds[i].z = rand_z * (float)depth_z;
        uint32_t hash = seeds[i].id ^ seed;
        hash = (hash ^ 61) ^ (hash >> 16);
        hash = hash + (hash << 3);
        hash = hash ^ (hash >> 4);
        hash = hash * 0x27d4eb2d;
        hash = hash ^ (hash >> 15);
        float random_0_1 = (float)(hash & 0xFFFFFF) / 16777215.0f;
        seeds[i].hauteur_max = 0.25f + (0.75f * random_0_1);
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
            int32_t seed_proche=0;

            for (int32_t i = 0; i < num_islands; i++) {
                float dx = fabsf(seeds[i].x - fx_deforme);
                float dz = fabsf(seeds[i].z - fz_deforme);

                float dist_euclidienne_carre = (dx * dx) + (dz * dz);
                float dist_manhattan = dx + dz;
                float dist_manhattan_carre = dist_manhattan * dist_manhattan;

                /* Mix structurel */
                float dist_mixte_carre = (dist_euclidienne_carre * 0.75f) + (dist_manhattan_carre * 0.25f)*seeds[i].hauteur_max;

                if (dist_mixte_carre < min_dist_hybride_carre) {
                    min_dist_hybride_carre = dist_mixte_carre;
                    seed_proche=i;
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


// --- Pseudo-Bruit de Perlin 2D ultra-léger pour le déterminisme ---
// Note : En production, on utilisera notre implémentation de la Tâche 2.3
static float hash2d(int x, int y) {
    int h = x * 374761393 + y * 668265263;
    h = (h ^ (h >> 13)) * 1274126177;
    return (float)(h & 0x7FFFFFFF) / 2147483647.0f;
}

static float bruit_gradient_2d(float x, float y) {
    int ix = (int)floorf(x);
    int iy = (int)floorf(y);
    float fx = x - (float)ix;
    float fy = y - (float)iy;

    // Interpolation cubique (Smoothstep local)
    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uy = fy * fy * (3.0f - 2.0f * fy);

    float a = hash2d(ix, iy);
    float b = hash2d(ix + 1, iy);
    float c = hash2d(ix, iy + 1);
    float d = hash2d(ix + 1, iy + 1);

    return a * (1.0f - ux) * (1.0f - uy) +
           b * ux * (1.0f - uy) +
           c * (1.0f - ux) * uy +
           d * ux * uy;
}

// --- Fonction Smootherstep de Ken Perlin ---
static float smootherstep(float edge0, float edge1, float x) {
    // Clamping de la valeur entre 0 et 1
    float t = (x - edge0) / (edge1 - edge0);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 0.0f; // Si t > 1, on est hors zone (bord absolu)
    if (x >= edge1) return 1.0f;
    
    // Formule : 6t^5 - 15t^4 + 10t^3
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// --- LA FONCTION DEMANDÉE ---
float calculer_masque_bordure_organique(float x, float y) {
    // 1. Sécurité : Clamping strict aux limites du monde
    if (x <= 0.0f || x >= ZYN_WORLD_MACRO_WIDTH_X  || y <= 0.0f || y >= ZYN_WORLD_MACRO_DEPTH_Z) {
        return 0.0f; 
    }

    // 2. Injection de distorsion (Domain Warping) pour casser la ligne droite
    // On utilise une basse fréquence (ex: division par 15000 mètres)
    float distorsion_x = bruit_gradient_2d(x / 30.0f, y / 30.0f) * 4.0f;
    float distorsion_y = bruit_gradient_2d(y / 30.0f, x / 30.0f) * 4.0f;

    float x_deforme = x + distorsion_x;
    float y_deforme = y + distorsion_y;

    // 3. Calcul de la distance la plus courte par rapport aux 4 bords du monde
    float dist_gauche = x_deforme;
    float dist_droite = ZYN_WORLD_MACRO_WIDTH_X  - x_deforme;
    float dist_bas    = y_deforme;
    float dist_haut   = ZYN_WORLD_MACRO_DEPTH_Z - y_deforme;

    // Trouver le minimum des 4 distances
    float dist_min = dist_gauche;
    if (dist_droite < dist_min) dist_min = dist_droite;
    if (dist_bas < dist_min)    dist_min = dist_bas;
    if (dist_haut < dist_min)   dist_min = dist_haut;

    // 4. Application du filtre de transition non linéaire
    // De 0m (bord) à TRANSITION_ZONE_M (plein continent), on applique le smootherstep
    float coefficient_masque = smootherstep(0.0f, ZYN_BAND_SIZE, dist_min);

    return coefficient_masque;
}

void zyn_map_relief_perlin(float * __restrict hauteurs_triees,const float * __restrict masque_voronoi,int32_t width_x,int32_t depth_z,uint32_t seed,float *out_relief_min,float *out_relief_max) {
// Configuration fixe de la génération de Zynthar
    const int32_t octaves = 5;
    const float persistence = 0.54f;
    const float lacunarity = 2.1f;
    const float base_scale = 0.006f;
    
    // Calcul déterministe des offsets via la seed globale (Brique 5.2)
    const float offset_x = 1250.5f + (float)(seed % 7919);
    const float offset_z = -4580.2f - (float)((seed >> 4) % 5417);

    // Registres locaux pour éviter les lectures/écritures répétées dans les pointeurs
    float relief_min = 0.0f;
    float relief_max = 0.0f;

    float * __restrict p_hauteurs = hauteurs_triees;
    const float * __restrict p_masque = masque_voronoi;

    // Boucle externe Z (Row-Major, respect absolu de la localité spatiale L1/L2)
    for (int32_t z = 0; z < depth_z; z++) {
        const float z_f = (float)z;
        
        // Invariants de boucle pré-calculés pour la ligne courante
        const float warp_dx_base_z = (z_f - 300.45f) * 0.002f;
        const float warp_dz_base_z = (z_f + 800.89f) * 0.002f;
        const float nz = (z_f + offset_z) * base_scale;

        const size_t row_index = (size_t)z * (size_t)width_x;

        #pragma GCC ivdep
        for (int32_t x = 0; x < width_x; x++) {
            const size_t index = row_index + x; 
            const float x_f = (float)x;
            float B_mask = calculer_masque_bordure_organique(x, z);
            
            // 1. Domain Warping stabilisé
            const float warp_dx = zyn_noise2d((x_f + 500.23f) * 0.002f, warp_dx_base_z) * 90.0f;
            const float warp_dz = zyn_noise2d((x_f - 200.67f) * 0.002f, warp_dz_base_z) * 90.0f;

            const float nx = (x_f + warp_dx + offset_x) * base_scale;
            const float final_nz = nz + (warp_dz * base_scale); 
        
            // 2. Échantillonnage du bruit fractal
            const float bruit_global = zyn_fractal_noise2d(nx, final_nz, octaves, persistence, lacunarity);

            // 3. Application de l'équation morphologique globale
            const float v_mask = p_masque[index];
            const float zone_oceanique = 1.0f - v_mask;
            float signal_falaise = bruit_global; 

            // Étape A : On amplifie le contraste du bruit à un certain niveau pour créer des ruptures
            if (signal_falaise > 0.2f && signal_falaise < 0.5f) {
                // On est dans la zone de falaise potentielle. 
                // On applique une interpolation cubique (Hermite) pour redresser violemment la pente
                float t = (signal_falaise - 0.2f) / 0.3f; // Normalisation entre 0 et 1
                float courbe_accentuee = t * t * (3.0f - 2.0f * t); // Smoothstep
    
                // On applique l'étirage vertical (ici bridé à une amplitude maximale équivalente à 200m dans ton échelle)
                // On réinjecte une micro-perturbation pour que la falaise ne soit pas un mur lisse et rectiligne
                float perturbation_falaise = warp_dx * 0.05f; 
                signal_falaise = 0.02f + (courbe_accentuee * 0.05f) + 0.1*perturbation_falaise;
            }
            
            float amplitude_falaise = 0.5f * B_mask;
            float amplitude_bruit   = 3.5f * B_mask;
            
            float relief_terrestre = (amplitude_falaise * signal_falaise) + (bruit_global * amplitude_bruit) + (v_mask * v_mask * 2.5f);
            
            float ocean_local = zone_oceanique * zone_oceanique * 0.6f;
            float abysse_bord = (1.0f - B_mask) * 4.0f;
            
            // 5. Équation finale équilibrée
            float relief_brut = relief_terrestre - ocean_local - abysse_bord;
            
          //  const float relief_brut = 0.5f*signal_falaise+(bruit_global * 3.5f) + (v_mask * v_mask* 2.5f) - (zone_oceanique * zone_oceanique * 0.6f);

            // 4. Accumulation native min/max (Branchless)
            relief_min = __builtin_fminf(relief_brut, relief_min);
            relief_max = __builtin_fmaxf(relief_brut, relief_max);
        
            p_hauteurs[index] = relief_brut;
        }
    }

    // Exportation finale des extrema vers les pointeurs appelants
    *out_relief_min = relief_min;
    *out_relief_max = relief_max;
}

float zyn_niv_mer_corrige(float* hauteurs_triees, size_t total_cases, double max_sea_percentage) {
    if (hauteurs_triees == NULL || total_cases == 0) {
        return 0.0f;
    }

    float* copie_pour_tri = (float*)malloc(total_cases * sizeof(float));
    if (copie_pour_tri == NULL) { 
        return 0.0f; 
    }
    
    memcpy(copie_pour_tri, hauteurs_triees, total_cases * sizeof(float));
    qsort(copie_pour_tri, total_cases, sizeof(float), comparer_floats);

    size_t index_mer = (size_t)((double)total_cases * max_sea_percentage);
    index_mer = __builtin_fminf(index_mer, total_cases-1);
    
    float niveau_mer_calcule = copie_pour_tri[index_mer];
    free(copie_pour_tri);

    return niveau_mer_calcule;
}

void zyn_map_correction_niv_mer(float* __restrict hauteurs_triees, size_t total_cases, float niveau_mer_calcule){

    float max_brut_terre = 0.0001f; 
    float min_brut_mer   = -0.0001f;

    #pragma GCC ivdep
    for (size_t i = 0; i < total_cases; i++) {
        const float alt_relative = hauteurs_triees[i] - niveau_mer_calcule;
        
        // Stockage direct du résultat de la soustraction (fusion de la boucle 2)
        hauteurs_triees[i] = alt_relative;

         max_brut_terre = __builtin_fmaxf(max_brut_terre, alt_relative);
        min_brut_mer   = __builtin_fminf(min_brut_mer, alt_relative);
    }
    
    normaliser(hauteurs_triees, min_brut_mer, max_brut_terre, total_cases, -1.0f, 1.0f);
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
        for (uint32_t z = 0; z < (uint32_t)depth_z; z++) {
            for (uint32_t x = 0; x < (uint32_t)width_x; x++) {
                size_t index = ZYN_INDEX(x, z, width_x);
                map[index].elevation_max_dm = M_TO_DM((int16_t)(masque_voronoi[index] * ZYN_WORLD_Y_MAX));
                map[index].region_x = (uint8_t)( x/256);
                map[index].region_z = (uint8_t)( z/256);               
                map[index].chunk_x = (uint8_t)( x%256);
                map[index].chunk_z = (uint8_t)( z%256);
            }
        }
        free(masque_voronoi);
        test_config->early_exit=1;
        return; /* COURT-CIRCUIT : On ne fait pas le Perlin, ni le qsort */
    }
    size_t total_cases = (size_t)width_x * (size_t)depth_z;
    float* hauteurs_triees = (float*)malloc(total_cases * sizeof(float));
    if (hauteurs_triees == NULL) { free(masque_voronoi); return; }
    float relief_min=0.0f;
    float relief_max=0.0f;
    
    zyn_map_relief_perlin(hauteurs_triees,masque_voronoi,width_x,depth_z,seed,&relief_min,&relief_max);

    free(masque_voronoi);

    normaliser(hauteurs_triees, relief_min,relief_max,total_cases,-1.0f,1.0f);
    
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
                map[index].region_x = (uint8_t)( x/256);
                map[index].region_z = (uint8_t)( z/256);               
                map[index].chunk_x = (uint8_t)( x%256);
                map[index].chunk_z = (uint8_t)( z%256);
            }
        }
        free(hauteurs_triees);
        test_config->early_exit=1;
        return; 
    }

    float niveau_mer_calcule = zyn_niv_mer_corrige(hauteurs_triees, total_cases, max_sea_percentage); 
    zyn_map_correction_niv_mer(hauteurs_triees, total_cases, niveau_mer_calcule);
    
    normaliser(hauteurs_triees, -1.0f,1.0f,total_cases,(float)ZYN_WORLD_Y_MIN,(float)ZYN_WORLD_Y_MAX);

    for (int32_t z = 0; z < depth_z; z++) {
        for (int32_t x = 0; x < width_x; x++) {
            size_t index = ZYN_INDEX(x, z, width_x);
            map[index].region_x = (uint8_t)( x/256);
            map[index].region_z = (uint8_t)( z/256);               
            map[index].chunk_x = (uint8_t)( x%256);
            map[index].chunk_z = (uint8_t)( z%256);
            map[index].elevation_max_dm = (int16_t)roundf(hauteurs_triees[index] * 10.0f);
        }
    }

    free(hauteurs_triees);
    /* -------------------------------------------------------------------------
       INTERCEPTION ET COPIE DANS MAP SI ARRET PAS 3 DEMANDÉ
        ------------------------------------------------------------------------- */
    if (test_config != NULL && test_config->active_test == 1 && test_config->target_step == 3) {
        test_config->early_exit=1;
    }
}
/* =============================================================================
 * LISSAGE DES CÔTES PAR AUTOMATE CELLULAIRE ELEVE AU TYPE SIZE_T
 * ============================================================================= */
void zyn_gen_map_relief_smooth_coastlines(MacroChunk* map, int32_t width_x, int32_t depth_z, int32_t iterations,ZynTestConfig* test_config) {
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
        
        #pragma GCC ivdep
        for (size_t z = 1; z < depth_z - 1; z++) {
            const size_t offset_ligne = z * width_x;
            const size_t haut = offset_ligne - width_x;
            const size_t bas  = offset_ligne + width_x;

            // Chargement initial des colonnes pour le glissement (Pipeline logiciel manuel pour éliminer les redondances)
            // Permet d'éviter de relire 3 fois les mêmes cases en mémoire pour les colonnes adjacentes.
            
            #pragma GCC vectorize
            for (size_t x = 1; x < width_x - 1; x++) {
                // Somme brute des 8 voisins (en uint8_t ou int32_t automatique pour la SIMD)
                const int32_t voisins_terre = 
                    grille_binaire[haut + x - 1]         + grille_binaire[haut + x]         + grille_binaire[haut + x + 1] +
                    grille_binaire[offset_ligne + x - 1]                                    + grille_binaire[offset_ligne + x + 1] +
                    grille_binaire[bas + x - 1]          + grille_binaire[bas + x]          + grille_binaire[bas + x + 1];

                const size_t index_actuel = offset_ligne + x;
                const uint8_t etat_actuel = grille_binaire[index_actuel];

                // Élimination des branches (Branchless logic) :
                // condition 1 (Meurt d'isolement) : etat_actuel == 1 && voisins_terre < 4   -> devient 0
                // condition 2 (Naissance)         : etat_actuel == 0 && voisins_terre >= 5  -> devient 1
                // condition 3 (Survie)            : reste inchangé si voisins_terre == 4
                
                // Formule booléenne condensée compilable en instructions "CMOV" ou masques binaires SIMD :
                const uint8_t garde_terre = (etat_actuel & (voisins_terre >= 4));
                const uint8_t nait_terre  = (~etat_actuel & (voisins_terre >= 5));
                
                nouvelle_grille[index_actuel] = (garde_terre | nait_terre) & 1;
            }
        }
        
        // Optimisation majeure : On échange les pointeurs au lieu de faire un memcpy coûteux (Double Buffering)
        uint8_t* temp = grille_binaire;
        grille_binaire = nouvelle_grille;
        nouvelle_grille = temp;
    }

    // Si le nombre d'itérations est impair, les dernières données calculées sont dans 'nouvelle_grille'.
    // On effectue un unique memcpy final uniquement si nécessaire pour synchroniser le buffer d'origine.
    if (iterations & 1) {
        uint8_t* temp = grille_binaire;
        grille_binaire = nouvelle_grille;
        nouvelle_grille = temp;
    }
    #pragma GCC ivdep
    /* 3. Réassignation finale des hauteurs ajustées dans la structure map */
    for (int32_t z = 0; z < depth_z; z++) {
        #pragma GCC vectorize
        for (int32_t x = 0; x < width_x; x++) {
            size_t idx = ZYN_INDEX(x, z, width_x);
            const int16_t alt_dm = map[idx].elevation_max_dm;
            
            const uint8_t est_terre = grille_binaire[idx]; // 1 si Terre, 0 si Mer
            
            

            const int16_t mask = alt_dm >> 15;
            const int16_t alt_abs = (alt_dm ^ mask) - mask;

            // Calcul des deux trajectoires cibles de manière purement arithmétique
            const int16_t cible_terre = (alt_dm > 0) ? alt_dm : alt_abs + 1;
            const int16_t cible_mer   = (alt_dm < 0) ? alt_dm : -alt_abs - 1;

            // Sélection finale "Branchless" (génère une instruction CMOV ou un masquage SIMD)
            // Si est_terre vaut 1, on prend cible_terre. Si 0, on prend cible_mer.
            map[idx].elevation_max_dm = est_terre ? cible_terre : cible_mer;
        }
    }

    free(grille_binaire);
    free(nouvelle_grille);
    /* -------------------------------------------------------------------------
       INTERCEPTION ET COPIE DANS MAP SI ARRET PAS 4 DEMANDÉ
        ------------------------------------------------------------------------- */
    if (test_config != NULL && test_config->active_test == 1 && test_config->target_step == 4) {
        test_config->early_exit=1;
    }
}

/* =============================================================================
 * POINT D'ENTRÉE DU MODULE : NOMBRE D'ÎLES ET VARIATION TOTALE
 * ============================================================================= */
void zyn_gen_map_relief(MacroChunk* map, int32_t width_x, int32_t depth_z, uint32_t seed, ZynTestConfig* test_config) {
    if (map == NULL || width_x <= 0 || depth_z <= 0) return;
    zyn_noise_init(seed);
    
    double surface = (double)width_x * (double)depth_z;
    double base_iles = surface / 222222;
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
    zyn_gen_map_relief_smooth_coastlines(map, width_x, depth_z, 2,test_config);
}
