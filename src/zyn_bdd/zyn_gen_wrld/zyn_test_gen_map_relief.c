//////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                      //
//                                          ZYNTHAR v0.1                                                //
//                                                                                                      //
// Auteur : Dehem70                                                                                     //
// Date   : 28/05/2026                                                                                  //
//                                                                                                      //
// zyn_test_gen_map_relief  ; tests regression et performance pour zyn_gen_map_relief                   //
//                                                                                                      //
//////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include "zyn_noise.h"
#include "zyn_gen_map_relief.h"
#include "zyn_gen_png.h"
#include <../../include/zynthar.h>

#define SEED_MONDE           7777U
#define EPSILON              1e-5f

typedef struct {
    int32_t index;
    float alt_attendue;
} TemoinMap;

int main(void) {
    printf("=====================================================================\n");
    printf("     ZYNTHAR : MODULE DE TEST & BENCHMARK (zyn_gen_map_relief)       \n");
    printf("=====================================================================\n\n");

    clock_t start_global, end_global;
    clock_t start_etape, end_etape;
    double temps_cpu;

    start_global = clock();

    int32_t ZYN_X = ZYN_X_MAX/ZYN_CHUNK_MACRO_DIM;
    int32_t ZYN_Y = ZYN_Y_MAX/ZYN_CHUNK_MACRO_DIM;

    /* =========================================================================
     * PHASE 1 : ALLOCATION MÉMOIRE
     * ========================================================================= */
    printf("[1/4] Allocation de la grille (%dx%d)... ", ZYN_X, ZYN_Y);
    fflush(stdout);
    
    start_etape = clock();
    MacroChunk* map = zyn_gen_map_relief_alloc(ZYN_X, ZYN_Y);
    end_etape = clock();
    
    if (map == NULL) return EXIT_FAILURE;
    printf("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);

    /* =========================================================================
     * PHASE 2 : FUSION GÉOMORPHOLOGIQUE (VORONOI + FRACTAL)
     * ========================================================================= */
    printf("[2/4] Génération de l'archipel & calibration mer (4 îles, 55%% eau)... ");
    fflush(stdout);

    /* Réinitialisation de la graine pour garantir le déterminisme */
    zyn_noise_init(SEED_MONDE);

    start_etape = clock();
    /* Génération brute de l'archipel avec 45% de mer par défaut */
    zyn_gen_map_relief_archipelago(map, ZYN_X, ZYN_Y, 4, 0.55f);
    end_etape = clock();
    
    printf("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);

    /* =========================================================================
     * PHASE 3 : AUTOMATE CELLULAIRE (LISSAGE DES CÔTES)
     * ========================================================================= */
    printf("[3/4] Lissage des lignes de côtes (3 itérations)... ");
    fflush(stdout);

    start_etape = clock();
    zyn_gen_map_relief_smooth_coastlines(map, ZYN_X, ZYN_Y, 3);
    end_etape = clock();

    printf("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);

    /* =========================================================================
     * PHASE 4 : VÉRIFICATION DE LA NON-RÉGRESSION (DÉTERMINISME)
     * ========================================================================= */
    printf("\n[4/4] Vérification de la précision mathématique...\n");

    /* Points de contrôles arbitraires sur la grille de 500 000 éléments */
    TemoinMap temoins[] = {
        { 0,          0.115689f }, /* Bordure  */
        { 250500,    -0.085954f }, /* Milieu de carte  */
        { 499999,    0.009014f }  /* Fin de carte  */
    };
    
    int32_t erreurs = 0;
    for (int i = 0; i < 3; i++) {
        int32_t idx = temoins[i].index;
        float alt_obtenue = map[idx].elevation_max;

        if (fabsf(alt_obtenue - temoins[i].alt_attendue) > EPSILON) {
            fprintf(stderr, "  [ÉCHEC] MacroChunk #%d : obtenu %f, attendu %f\n", 
                    idx, alt_obtenue, temoins[i].alt_attendue);
            erreurs++;
        }
    }

    if (erreurs == 0) {
        printf("  [SUCCÈS] La géomorphologie macro est parfaitement stable et déterministe.\n");
    } else {
        printf("  [ALERTE] Malédiction ! %d écart(s) détecté(s) sur le relief témoin.\n", erreurs);
    }

/* =========================================================================
     * EXPORT VISUEL EN PNG
     * ============================================================================= */
    printf("\nExportation de la carte en image PNG (carte_elevation.png)... ");
    fflush(stdout);
    if (zyn_gen_png_elevation(map, ZYN_X, ZYN_Y, "carte_elevation.png","carte_elevation_bin.png")) {
        printf("OK !\n");
    } else {
        printf("[ÉCHEC]\n");
    }
    
    /* Nettoyage de la mémoire */
    zyn_gen_map_relief_free(map);

    end_global = clock();
    temps_cpu = ((double)(end_global - start_global)) / CLOCKS_PER_SEC;

    printf("\n=====================================================================\n");
    printf(" PERFORMANCE GLOBALE : Genérée en %.4f secondes (Grille complète)\n", temps_cpu);
    printf("=====================================================================\n");

    return (erreurs == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
