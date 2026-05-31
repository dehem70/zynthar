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

/* Structure interne locale pour stocker les positions des germes d'îles (Plan horizontal X/Z) */
typedef struct {
    float x;
    float z;
} IslandSeed;

/* =============================================================================
 * GESTION DE LA MÉMOIRE (ALLOCATION CONTIGUË)
 * ============================================================================= */

MacroChunk* zyn_gen_map_relief_alloc(int32_t width_x, int32_t depth_z) {
    /* Validation de sécurité sur les dimensions de l'univers macro */
    if (width_x <= 0 || depth_z <= 0) {
        fprintf(stderr, "[ERREUR] Dimensions d'allocation invalides : %dx%d\n", width_x, depth_z);
        return NULL;
    }

    /* Calcul du nombre total de MacroChunks à allouer sur la grille horizontale */
    size_t total_chunks = (size_t)width_x * (size_t)depth_z;

    /* Allocation d'un bloc unique et initialisation à zéro (calloc) */
    MacroChunk* map = (MacroChunk*)calloc(total_chunks, sizeof(MacroChunk));
    if (map == NULL) {
        fprintf(stderr, "[ERREUR] Échec de l'allocation mémoire pour la carte macro (%zu éléments).\n", total_chunks);
        return NULL;
    }

    return map;
}

void zyn_gen_map_relief_free(MacroChunk* map) {
    if (map != NULL) {
        free(map);
    }
}

/* =============================================================================
 * GENERATION DU MASQUE DE VORONOI
 * ============================================================================= */

float* zyn_gen_map_relief_voronoi(int32_t width_x, int32_t depth_z, int32_t num_islands) {
    
    size_t total_cases = (size_t)width_x * (size_t)depth_z;
    
    // Conteneur temporaire Haute-Fidélité
    float* masque_voronoi = (float*)malloc(total_cases * sizeof(float));
    if (masque_voronoi == NULL) return NULL;
    
    /* Allocation d'un tableau de germes sur le tas pour éviter de saturer la pile */
    IslandSeed* seeds = (IslandSeed*)malloc(sizeof(IslandSeed) * num_islands);
    if (seeds == NULL) {
        // [CORRECTION] Libération du premier buffer et retour de NULL 
        free(masque_voronoi);
        return NULL;
    }

    /* Utilisation d'un mini-générateur déterministe local (LCG) pour placer les îles */
    uint32_t lcg_state = 54321U; 
    for (int32_t i = 0; i < num_islands; i++) {
        lcg_state = lcg_state * 1103515245 + 12345;
        seeds[i].x = (float)(lcg_state % width_x);

        lcg_state = lcg_state * 1103515245 + 12345;
        seeds[i].z = (float)(lcg_state % depth_z);
    }

    /* Définition du rayon d'action maximal théorique d'une île */
    float max_dist = (width_x < depth_z ? width_x : depth_z) / 3.5f; 

    for (int32_t z = 0; z < depth_z; z++) {
        for (int32_t x = 0; x < width_x; x++) {
            float min_dist_carre = 99999999.0f;

            for (int32_t i = 0; i < num_islands; i++) {
                float dx = seeds[i].x - (float)x;
                float dz = seeds[i].z - (float)z;
                float dist_carre = (dx * dx) + (dz * dz);
                if (dist_carre < min_dist_carre) min_dist_carre = dist_carre;
            }

            float min_dist = sqrtf(min_dist_carre);
            float val = 1.0f - (min_dist / max_dist);
            if (val < 0.0f) val = 0.0f;

            // Écriture directe en float pur [0.0f, 1.0f] : Zéro perte de précision !
            int32_t index = z * width_x + x;
            masque_voronoi[index] = val;
        }
    }

    return masque_voronoi; // On passe le conteneur à la fonction suivante
}

/* =============================================================================
 * UTILITAIRE DE COMPARAISON POUR LE TRI (QSORT)
 * ============================================================================= */
static int comparer_floats(const void* a, const void* b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    if (fa < fb) return -1;
    if (fa > fb) return 1;
    return 0;
}

/* =============================================================================
 * FUSION DU RELIEF & CALIBRAGE DU NIVEAU DE LA MER
 * ============================================================================= */
void zyn_gen_map_relief_archipelago(MacroChunk* map, int32_t width_x, int32_t depth_z, int32_t num_islands, float max_sea_percentage) {
    if (map == NULL || width_x <= 0 || depth_z <= 0) return;

    // 1. Récupération du conteneur temporaire Voronoi en haute résolution
    float* masque_voronoi = zyn_gen_map_relief_voronoi(width_x, depth_z, num_islands);
    if (masque_voronoi == NULL) return;

    size_t total_cases = (size_t)width_x * (size_t)depth_z;
    float* hauteurs_triees = (float*)malloc(total_cases * sizeof(float));
    
    if (hauteurs_triees == NULL) return;

    int32_t octaves = 5;
    float persistence = 0.54f;
    float lacunarity = 2.1f;
    float base_scale = 0.006f;
    
    float offset_x = 1250.5f;
    float offset_z = -4580.2f;

    float warp_scale = 0.002f;
    float warp_intensity = 40.0f;

    /* 2. Premier parcours : Fusion mathématique 100% Floats */
    for (int32_t z = 0; z < depth_z; z++) {
        for (int32_t x = 0; x < width_x; x++) {
            size_t index = (size_t)z * width_x + x;

            // Calcul du Domain Warping spatiale
            float warp_dx = zyn_noise2d(((float)x + 500.0f) * 0.002f, ((float)z - 300.0f) * 0.002f) * 40.0f;
            float warp_dz = zyn_noise2d(((float)x - 200.0f) * 0.002f, ((float)z + 800.0f) * 0.002f) * 40.0f;

            float nx = ((float)x + warp_dx + offset_x) * base_scale;
            float nz = ((float)z + warp_dz + offset_z) * base_scale;
            float relief_fractal = zyn_fractal_noise2d(nx, nz, octaves, persistence, lacunarity);
            
            // 1. Ton bruit fractal global standard
            float bruit_global = zyn_fractal_noise2d(nx, nz, octaves, persistence, lacunarity);

            // 2. Récupération du Voronoi pur
            float v_mask = masque_voronoi[index];
            float v_mask_accentue = v_mask * v_mask;

            // 3. L'inverse du Voronoi pour cibler la pleine mer
            float zone_oceanique = 1.0f - v_mask;

            // 4. FUSION DYNAMIQUE :
            // - Le Voronoi (v_mask_accentue * 1.4f) pousse les îles vers le haut (+2000m)
            // - L'océan (zone_oceanique * -0.8f) creuse activement les fosses vers le bas (-1000m)
            // - Le bruit apporte la rugosité partout sans être annulé.
            float relief_brut = (bruit_global * 0.5f) + (v_mask_accentue * 1.4f) - (zone_oceanique * 0.8f);

            hauteurs_triees[index] = relief_brut;
        }
    }

    // Le masque de Voronoi a été entièrement consommé sans jamais avoir été altéré,
    // on peut libérer le conteneur immédiatement pour libérer la RAM !
    free(masque_voronoi); 

    /* 3. Tri rapide (QuickSort) sur les floats de haute précision */
    // On duplique temporairement le tableau pour le trier sans perdre l'ordre géographique de hauteurs_triees
    float* copie_pour_tri = (float*)malloc(total_cases * sizeof(float));
    if (copie_pour_tri == NULL) {
        free(hauteurs_triees);
        return;
    }
    memcpy(copie_pour_tri, hauteurs_triees, total_cases * sizeof(float));

    qsort(copie_pour_tri, total_cases, sizeof(float), comparer_floats);

    /* 4. Identification du pivot sur le tableau trié */
    size_t index_mer = (size_t)(total_cases * max_sea_percentage);
    if (index_mer >= total_cases) index_mer = total_cases - 1;
    float niveau_mer_calcule = copie_pour_tri[index_mer];
    free(copie_pour_tri); // Plus besoin

    /* =========================================================================
     * ÉTAPE 5 : RECHERCHE DES EXTRÊMES ABSOLUS EN HAUTE PRÉCISION
     * ========================================================================= */
    float max_brut_terre = 0.0001f; // Évite la division par zéro si la carte est plate
    float min_brut_mer   = -0.0001f;

    for (size_t i = 0; i < total_cases; i++) {
        float alt_relative = hauteurs_triees[i] - niveau_mer_calcule;
        
        if (alt_relative > 0.0f && alt_relative > max_brut_terre) {
            max_brut_terre = alt_relative;
        }
        if (alt_relative < 0.0f && alt_relative < min_brut_mer) {
            min_brut_mer = alt_relative;
        }
    }
    
    /* Extraction des constantes dynamiques depuis zynthar.h (exprimées en mètres) */
    float max_monde_config_m  = (float)ZYN_WORLD_Y_MAX;
    float min_monde_config_m  = (float)ZYN_WORLD_Y_MIN;

    /* CALCUL DES COEFFICIENTS BASÉ SUR L'ESPACE DISPONIBLE REEL */
    float hauteur_terre_dispo  = fabsf(max_monde_config_m);
    float profondeur_mer_dispo = fabsf(min_monde_config_m);
    
    /* CALCUL DES COEFFICIENTS LINÉAIRES DE CORRECTION ASYMÉTRIQUE */
    float coef_positif = hauteur_terre_dispo / max_brut_terre;
    float coef_negatif = profondeur_mer_dispo / fabsf(min_brut_mer);

    /* =========================================================================
     * ÉTAPE 6 : ÉTALEMENT PAR MORCEAUX ET UNIQUE COMPRESSION FINALE
     * ========================================================================= */
    for (size_t index = 0; index < total_cases; index++) {
        float alt_relative = hauteurs_triees[index] - niveau_mer_calcule;
        float alt_finale_m = 0.0f ;

        /* Le coefficient étire le relief, et on l'ajoute au niveau de la mer de référence */
        if (alt_relative > 0.0f) {
            // On monte : niveau de la mer + (distance relative étirée)
            alt_finale_m = (alt_relative * coef_positif);
        } else if (alt_relative < 0.0f) {
            // On descend : niveau de la mer - (distance relative absolue étirée)
            alt_finale_m = -(fabsf(alt_relative) * coef_negatif);
        }

        // Bornage de sécurité dynamique basé sur la configuration de zynthar.h
        float limite_basse = fminf(min_monde_config_m, max_monde_config_m);
        float limite_haute = fmaxf(min_monde_config_m, max_monde_config_m);
        float alt_clamped_m = fmaxf(limite_basse, fminf(alt_finale_m, limite_haute));

        /* L'unique arrondi de la chaîne : passage en int16_t décimètres */
        map[index].elevation_max_dm = (int16_t)roundf(alt_clamped_m * 10.0f);
        map[index].chunk_x = (int32_t)(index % width_x);
        map[index].chunk_z = (int32_t)(index / width_x);
    }

    free(hauteurs_triees);
}
/* =============================================================================
 * LISSAGE DES CÔTES PAR AUTOMATE CELLULAIRE (VOISINAGE DE MOORE)
 * ============================================================================= */

void zyn_gen_map_relief_smooth_coastlines(MacroChunk* map, int32_t width_x, int32_t depth_z, int32_t iterations) {
    if (map == NULL || width_x <= 0 || depth_z <= 0 || iterations <= 0) return;

    size_t total_cases = (size_t)width_x * (size_t)depth_z;

    /* 1. Allocation des buffers de masques binaires (1 octet par case = ultra léger pour le CPU) */
    uint8_t* grille_binaire = (uint8_t*)malloc(total_cases * sizeof(uint8_t));
    if (grille_binaire == NULL) return;

    /* Initialisation : 1 pour la terre ferme (altitude positive), 0 pour la mer */
    for (size_t i = 0; i < total_cases; i++) {
        grille_binaire[i] = (map[i].elevation_max_dm > 0) ? 1 : 0;
    }

    uint8_t* nouvelle_grille = (uint8_t*)malloc(total_cases * sizeof(uint8_t));
    if (nouvelle_grille == NULL) {
        free(grille_binaire);
        return;
    }
    memcpy(nouvelle_grille, grille_binaire, total_cases);

    /* 2. Boucle principale de l'automate cellulaire */
    for (int32_t iter = 0; iter < iterations; iter++) {
        
        /* On ignore les bordures extérieures pour éliminer tout risque de débordement mémoire */
        for (int32_t z = 1; z < depth_z - 1; z++) {
            // Pré-calcul de l'index de ligne pour économiser des multiplications lourdes
            int32_t offset_ligne = z * width_x;
            int32_t haut = offset_ligne - width_x;
            int32_t bas = offset_ligne + width_x;

            for (int32_t x = 1; x < width_x - 1; x++) {
                /* Déroulage complet du voisinage de Moore (Aucune boucle dx/dz, accès RAM direct) */
                int32_t voisins_terre = 
                    grille_binaire[haut + x - 1] + grille_binaire[haut + x] + grille_binaire[haut + x + 1] +
                    grille_binaire[offset_ligne + x - 1]                   + grille_binaire[offset_ligne + x + 1] +
                    grille_binaire[bas + x - 1]  + grille_binaire[bas + x]  + grille_binaire[bas + x + 1];

                int32_t index_actuel = offset_ligne + x;
                
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

    /* 3. Réapplication du masque lissé sur les altitudes compressées (décimètres) */
    for (size_t i = 0; i < total_cases; i++) {
        int16_t alt_dm = map[i].elevation_max_dm;

        if (grille_binaire[i] == 1) {
            /* C'est de la terre ferme : on garantit une altitude strictement positive (min 1 décimètre = 10cm) */
            map[i].elevation_max_dm = (alt_dm > 0) ? alt_dm : abs(alt_dm) + 1;
        } else {
            /* C'est de l'eau : on garantit une altitude négative ou nulle (max -1 décimètre = -10cm) */
            map[i].elevation_max_dm = (alt_dm < 0) ? alt_dm : -abs(alt_dm) - 1;
        }
    }

    free(grille_binaire);
    free(nouvelle_grille);
}
