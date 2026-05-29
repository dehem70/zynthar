//////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                      //
//                                          ZYNTHAR v0.1                                                //
//                                                                                                      //
// Auteur : Dehem70                                                                                     //
// Date   : 28/05/2026                                                                                  //
//                                                                                                      //
// zyn_gen_map_relief  ; génération de la carte macro du relief                                         //
//                                                                                                      //
//////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "zyn_gen_map_relief.h"
#include "zyn_noise.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

/* Structure interne locale pour stocker les positions des germes d'îles */
typedef struct {
    float x;
    float y;
} IslandSeed;

/* =============================================================================
 * GESTION DE LA MÉMOIRE (ALLOCATION CONTIGUË)
 * ============================================================================= */

MacroChunk* zyn_gen_map_relief_alloc(int32_t width, int32_t height) {
    /* Validation de sécurité sur les dimensions */
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "[ERREUR] Dimensions d'allocation invalides : %dx%d\n", width, height);
        return NULL;
    }

    /* Calcul du nombre total de MacroChunks à allouer */
    size_t total_chunks = (size_t)width * (size_t)height;

    /* Allocation d'un bloc unique et mise à zéro de la mémoire (calloc)
       Toutes les cases prendront par défaut la valeur 0 (et donc BIOME_INCONNU) */
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

void zyn_gen_map_relief_voronoi(MacroChunk* map, int32_t width, int32_t height, int32_t num_islands) {
    if (map == NULL || width <= 0 || height <= 0 || num_islands <= 0) return;

    /* Allocation d'un tableau de germes sur la pile (stack) pour une vitesse maximale */
    IslandSeed* seeds = (IslandSeed*)malloc(sizeof(IslandSeed) * num_islands);
    if (seeds == NULL) return;

    /* Utilisation d'un mini-générateur déterministe local pour placer les îles */
    uint32_t lcg_state = 54321U; 
    for (int32_t i = 0; i < num_islands; i++) {
        /* Calcul de X aléatoire entre 0 et width */
        lcg_state = lcg_state * 1103515245 + 12345;
        seeds[i].x = (float)(lcg_state % width);

        /* Calcul de Y aléatoire entre 0 et height */
        lcg_state = lcg_state * 1103515245 + 12345;
        seeds[i].y = (float)(lcg_state % height);
    }

    /* Définition du rayon d'action maximal théorique d'une île */
    float max_dist = (width < height ? width : height) / 3.5f; 

    /* Parcours de l'intégralité de la grille plate */
    for (int32_t y = 0; y < height; y++) {
        for (int32_t x = 0; x < width; x++) {
            float min_dist = 999999.0f;

            /* Recherche de la graine d'île la plus proche pour ce pixel macro */
            for (int32_t i = 0; i < num_islands; i++) {
                float dx = seeds[i].x - (float)x;
                float dy = seeds[i].y - (float)y;
                /* On calcule la distance Euclidienne brute */
                float dist = sqrtf(dx * dx + dy * dy);

                if (dist < min_dist) {
                    min_dist = dist;
                }
            }

            /* Calcul de l'atténuation linéaire (1.0 au centre, 0.0 à la périphérie) */
            float val = 1.0f - (min_dist / max_dist);
            if (val < 0.0f) val = 0.0f;

            /* Stockage temporaire du masque dans l'élévation */
            int32_t index = y * width + x;
            map[index].elevation_max = val;
        }
    }

    /* Libération du tableau temporaire des germes */
    free(seeds);
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

void zyn_gen_map_relief_archipelago(MacroChunk* map, int32_t width, int32_t height, int32_t num_islands, float max_sea_percentage) {
    if (map == NULL || width <= 0 || height <= 0) return;

    /* 1. On s'assure d'abord que le masque de Voronoi est bien calculé dans la grille */
    zyn_gen_map_relief_voronoi(map, width, height, num_islands);

    size_t total_cases = (size_t)width * (size_t)height;

    /* 2. Allocation d'un tableau plat temporaire pour stocker et trier toutes les hauteurs */
    float* hauteurs_triees = (float*)malloc(total_cases * sizeof(float));
    if (hauteurs_triees == NULL) return;

    /* Configurations des octaves du bruit */
    int32_t octaves = 5;
    float persistence = 0.5f;
    float lacunarity = 2.0f;
    
    /* Échelle et offsets arbitraires déterministes pour le bruit de relief */
    float base_scale = 0.004f;
    float offset_x = 1250.5f;
    float offset_y = -4580.2f;

    float intensite_ile = 1.2f;
    float niveau_mer_base = 0.7f;

    /* 3. Premier parcours : Calcul et fusion des reliefs bruts */
    for (int32_t y = 0; y < height; y++) {
        for (int32_t x = 0; x < width; x++) {
            size_t index = (size_t)y * width + x;

            /* Récupération du masque de Voronoi stocké temporairement dans l'élévation */
            float masque_voronoi = map[index].elevation_max;

            /* Calcul du bruit fractal continu */
            float nx = ((float)x + offset_x) * base_scale;
            float ny = ((float)y + offset_y) * base_scale;
            float relief_fractal = zyn_fractal_noise2d(nx, ny, octaves, persistence, lacunarity);

            /* Formule de fusion mathématique */
            float relief_brut = relief_fractal + (masque_voronoi * intensite_ile) - niveau_mer_base;

            /* Sauvegarde dans le tableau de tri et mise à jour temporaire de la carte */
            hauteurs_triees[index] = relief_brut;
            map[index].elevation_max = relief_brut;
        }
    }


    /* 4. Tri rapide (QuickSort) de l'ensemble des hauteurs de l'univers */
    qsort(hauteurs_triees, total_cases, sizeof(float), comparer_floats);

    /* 5. Identification de la valeur pivot correspondant au ratio de mer recherché */
    size_t index_mer = (size_t)(total_cases * max_sea_percentage);
    if (index_mer >= total_cases) index_mer = total_cases - 1;
    float niveau_mer_calcule = hauteurs_triees[index_mer];

    /* 6. Second parcours : Ajustement final de la hauteur par rapport au pivot */
    for (size_t i = 0; i < total_cases; i++) {
        map[i].elevation_max -= niveau_mer_calcule;
        
        /* Injection des coordonnées géographiques réelles dans la structure */
        map[i].x = (int32_t)(i % width);
        map[i].y = (int32_t)(i / width);
    }

    /* Libération de la mémoire du tableau de tri */
    free(hauteurs_triees);
}


/* =============================================================================
 * LISSAGE DES CÔTES PAR AUTOMATE CELLULAIRE (VOISINAGE DE MOORE)
 * ============================================================================= */

void zyn_gen_map_relief_smooth_coastlines(MacroChunk* map, int32_t width, int32_t height, int32_t iterations) {
    if (map == NULL || width <= 0 || height <= 0 || iterations <= 0) return;

    size_t total_cases = (size_t)width * (size_t)height;

    /* 1. Allocation du buffer de masque temporaire (1 octet par case = ultra léger) */
    uint8_t* grille_binaire = (uint8_t*)malloc(total_cases * sizeof(uint8_t));
    if (grille_binaire == NULL) return;

    /* 2. Initialisation du masque binaire : 1 pour la terre (> 0), 0 pour l'eau (<= 0) */
    for (size_t i = 0; i < total_cases; i++) {
        grille_binaire[i] = (map[i].elevation_max > 0.0f) ? 1 : 0;
    }

    /* Allocation d'un second buffer pour appliquer les règles de transition sans conflit */
    uint8_t* nouvelle_grille = (uint8_t*)malloc(total_cases * sizeof(uint8_t));
    if (nouvelle_grille == NULL) {
        free(grille_binaire);
        return;
    }
    /* Copie de base pour garder les bordures intactes */
    memcpy(nouvelle_grille, grille_binaire, total_cases);

    /* 3. Boucle principale de l'automate cellulaire */
    for (int32_t iter = 0; iter < iterations; iter++) {
        
        /* On ignore les bordures extérieures (1 pixel de marge) pour éviter les débordements mémoire */
        for (int32_t y = 1; y < height - 1; y++) {
            for (int32_t x = 1; x < width - 1; x++) {
                int32_t voisins_terre = 0;

                /* Comptage des 8 voisins de Moore autour de la case (x, y) */
                for (int32_t dy = -1; dy <= 1; dy++) {
                    for (int32_t dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue; /* On ne se compte pas soi-même */

                        int32_t nx = x + dx;
                        int32_t ny = y + dy;
                        if (grille_binaire[ny * width + nx] == 1) {
                            voisins_terre++;
                        }
                    }
                }

                int32_t index_actuel = y * width + x;
                
                /* Application des règles de transition de ta maquette Python */
                if (grille_binaire[index_actuel] == 1 && voisins_terre < 4) {
                    nouvelle_grille[index_actuel] = 0; /* Érosion : la terre isolée devient de l'eau */
                } else if (grille_binaire[index_actuel] == 0 && voisins_terre >= 5) {
                    nouvelle_grille[index_actuel] = 1; /* Comblement : l'eau isolée devient de la terre */
                } else {
                    nouvelle_grille[index_actuel] = grille_binaire[index_actuel]; /* Statu quo */
                }
            }
        }

        /* Mise à jour du buffer de référence pour la prochaine itération */
        memcpy(grille_binaire, nouvelle_grille, total_cases);
    }

    /* 4. Réapplication du masque lissé sur les altitudes de la carte */
    for (size_t i = 0; i < total_cases; i++) {
        float alt = map[i].elevation_max;

        if (grille_binaire[i] == 1) {
            /* C'est de la terre ferme : on s'assure que l'altitude est strictement positive */
            map[i].elevation_max = (alt > 0.0f) ? alt : fabsf(alt) + 0.01f;
        } else {
            /* C'est de l'eau : on s'assure que l'altitude est négative ou nulle */
            map[i].elevation_max = (alt < 0.0f) ? alt : -fabsf(alt) - 0.01f;
        }
    }

    /* Libération de la mémoire des buffers d'automate */
    free(grille_binaire);
    free(nouvelle_grille);
}
