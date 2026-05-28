//////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                      //
//                                          ZYNTHAR v0.1                                                //
//                                                                                                      //
// Auteur : Dehem70                                                                                     //
// Date   : 28/05/2026                                                                                  //
//                                                                                                      //
// zyn_test_noise  ;  tests de regression et de performance de zyn_noise                                //
//                                                                                                      //
//////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "zyn_noise.h"

#include <../../include/zynthar.h>

/* Configurations du benchmark */
#define NB_ITERATIONS_BENCH  10000000  /* 10 Millions d'appels */
#define SEED_TEST            12345U

/* Tolérance pour la comparaison des nombres flottants (Single Precision EPSILON) */
#define EPSILON              1e-5f

/**
 * @brief Structure de témoin pour les tests de non-régression
 */
typedef struct {
    float x;
    float y;
    float z;
    float attendu;
} ValeurTemoin;

int main(void) {
    printf("=====================================================================\n");
    printf("         ZYNTHAR : MODULE DE TEST & BENCHMARK (zyn_noise)            \n");
    printf("=====================================================================\n\n");

    /* =========================================================================
     * PHASE 1 : TEST DE NON-RÉGRESSION (DÉTERMINISME & PRÉCISION)
     * ========================================================================= */
    printf("[1/2] Lancement des tests de non-régression...\n");
    zyn_noise_init(SEED_TEST);

    /* Valeurs témoins calculées à partir de l'implémentation de référence */
    ValeurTemoin temoins_2d[] = {
        { 0.0f,   0.0f,   0.0f,  0.000000f },
        { 12.5f, -3.7f,   0.0f, -0.451076f },
        { 45.12f, 89.67f, 0.0f,  0.187393f }
    };

    ValeurTemoin temoins_3d[] = {
        { 0.0f,   0.0f,    0.0f,   0.000000f },
        { 1.25f,  4.85f,  -2.3f,  -0.004531f },
        { 99.1f, -42.55f,  18.75f, 0.421403f }
    };

    int32_t erreurs = 0;

    /* Validation Bruit 2D */
    for (int i = 0; i < 3; i++) {
        float res = zyn_noise2d(temoins_2d[i].x, temoins_2d[i].y);
        if (fabsf(res - temoins_2d[i].attendu) > EPSILON) {
            fprintf(stderr, "  [ÉCHEC] 2D au point (%f, %f) : obtenu %f, attendu %f\n", 
                    temoins_2d[i].x, temoins_2d[i].y, res, temoins_2d[i].attendu);
            erreurs++;
        }
    }

    /* Validation Bruit 3D */
    for (int i = 0; i < 3; i++) {
        float res = zyn_noise3d(temoins_3d[i].x, temoins_3d[i].y, temoins_3d[i].z);
        if (fabsf(res - temoins_3d[i].attendu) > EPSILON) {
            fprintf(stderr, "  [ÉCHEC] 3D au point (%f, %f, %f) : obtenu %f, attendu %f\n", 
                    temoins_3d[i].x, temoins_3d[i].y, temoins_3d[i].z, res,temoins_3d[i].attendu);
            erreurs++;
        }
    }

    if (erreurs == 0) {
        printf("  [SUCCÈS] Tous les tests de non-régression sont au vert. Le moteur est stable.\n\n");
    } else {
        printf("  [ALERTE] %d anomalie(s) détectée(s) lors de la vérification mathématique.\n\n", erreurs);
    }

    /* =========================================================================
     * PHASE 2 : PROFILAGE ET PERFORMANCE (BENCHMARK)
     * ========================================================================= */
    printf("[2/2] Lancement du benchmark de stress-test (%d itérations)...\n", NB_ITERATIONS_BENCH);
    
    volatile float accumulation = 0.0f; /* Volatile pour empêcher le compilateur d'optimiser en supprimant la boucle */
    clock_t start, end;
    double temps_cpu;

    /* --- Benchmark 2D --- */
    printf("  Stressing zyn_noise2d... ");
    fflush(stdout);
    
    start = clock();
    for (int32_t i = 0; i < NB_ITERATIONS_BENCH; i++) {
        /* On fait varier légèrement les coordonnées pour éviter les effets de cache de calcul */
        accumulation += zyn_noise2d((float)i * 0.001f, (float)i * 0.0015f);
    }
    end = clock();
    
    temps_cpu = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Terminé en %.4f secondes.\n", temps_cpu);
    printf("  -> Vitesse 2D : %.2f Millions d'appels/sec\n\n", (NB_ITERATIONS_BENCH / 1000000.0) / temps_cpu);

    /* --- Benchmark 3D --- */
    printf("  Stressing zyn_noise3d... ");
    fflush(stdout);
    
    start = clock();
    for (int32_t i = 0; i < NB_ITERATIONS_BENCH; i++) {
        accumulation += zyn_noise3d((float)i * 0.001f, (float)i * 0.0012f, (float)i * 0.0015f);
    }
    end = clock();
    
    temps_cpu = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Terminé en %.4f secondes.\n", temps_cpu);
    printf("  -> Vitesse 3D : %.2f Millions d'appels/sec\n\n", (NB_ITERATIONS_BENCH / 1000000.0) / temps_cpu);

    /* Utilisation factice de l'accumulation pour satisfaire les exigences du compilateur */
    if (accumulation == 999999.0f) { printf("%f", accumulation); }

    printf("=====================================================================\n");
    printf("Fin du profilage.\n");
    printf("=====================================================================\n");

    return (erreurs == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
