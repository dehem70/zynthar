/* =============================================================================
 *
 * ZYNTHAR v0.1
 *
 * Auteur : Dehem70
 * Date   : 28/05/2026
 *
 * zyn_test_noise : Tests de non-régression et benchmark de performance de zyn_noise.
 * Génère automatiquement un rapport horodaté dans /reports/benchmarks/
 *
 * =============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include <zynthar.h>
#include "zyn_noise.h"

/* Configurations du benchmark */
#define NB_ITERATIONS_BENCH  10000000  /* 10 Millions d'appels */
#define SEED_TEST            12345U

/* Tolérance pour la comparaison des nombres flottants (Single Precision EPSILON) */
#define EPSILON              1e-5f

/**
 * @brief Structure de témoin pour les tests de non-régression.
 */
typedef struct {
    float x;
    float z;
    float y;
    float attendu;
} ValeurTemoin;

int main(void) {
    // 1. Récupération de la racine du projet
    char *root_env = getenv("ZYNTHAR_ROOT");
    if (root_env == NULL) {
        fprintf(stderr, "[-] Erreur : La variable d'environnement ZYNTHAR_ROOT n'est pas définie.\n");
        fprintf(stderr, "💡 Exécutez : source ~/.bashrc\n");
        return EXIT_FAILURE;
    }

    // 2. Récupération de la date et heure courantes
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    if (tm_info == NULL) {
        fprintf(stderr, "[-] Erreur : Impossible de récupérer l'heure locale.\n");
        return EXIT_FAILURE;
    }

    // Formatage de la date et de l'heure : AAAAMMJJ_HHMMSS
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

    // 3. Construction du chemin absolu du fichier de rapport
    char report_path[1024];
    snprintf(report_path, sizeof(report_path), "%s/reports/benchmarks/zyn_test_noise_%s.txt", root_env, timestamp);

    // 4. Ouverture du fichier de rapport en écriture
    FILE *report = fopen(report_path, "w");
    if (report == NULL) {
        fprintf(stderr, "[-] Erreur : Impossible de créer le fichier de rapport dans :\n    %s\n", report_path);
        fprintf(stderr, "💡 Vérifiez que l'arborescence /reports/tests/determinism/ existe.\n");
        return EXIT_FAILURE;
    }

    // Macro pratique pour écrire simultanément sur l'écran et dans le fichier
    #define PRINT_BOTH(...) do { printf(__VA_ARGS__); fprintf(report, __VA_ARGS__); } while(0)

    PRINT_BOTH("=====================================================================\n");
    PRINT_BOTH("         ZYNTHAR : RAPPORT DE TEST & BENCHMARK (zyn_noise)           \n");
    PRINT_BOTH("=====================================================================\n");
    PRINT_BOTH(" Fichier généré : zyn_test_noise_%s.txt\n", timestamp);
    PRINT_BOTH(" Chemin absolu  : %s\n", report_path);
    PRINT_BOTH("=====================================================================\n\n");

    /* =========================================================================
     * PHASE 1 : TEST DE NON-RÉGRESSION (DÉTERMINISME & PRÉCISION)
     * ========================================================================= */
    PRINT_BOTH("[1/2] Lancement des tests de non-régression...\n");
    zyn_noise_init(SEED_TEST);

    /* Valeurs témoins calculées à partir de l'implémentation de référence (Plan X, Z) */
    ValeurTemoin temoins_2d[] = {
        { 0.0f,   0.0f,   0.0f,  0.000000f },
        { 12.5f, -3.7f,   0.0f, -0.451076f },
        { 45.12f, 89.67f, 0.0f,  0.187393f }
    };

    /* Valeurs témoins 3D adaptées à l'ordre (X, Z, Y) */
    ValeurTemoin temoins_3d[] = {
        { 0.0f,   0.0f,    0.0f,   0.000000f },
        { 1.25f,  4.85f,  -2.3f,  -0.004531f }, 
        { 99.1f, -42.55f,  18.75f, 0.421403f }  
    };

    int32_t erreurs = 0;

    /* Validation Bruit 2D */
    for (int i = 0; i < 3; i++) {
        float res = zyn_noise2d(temoins_2d[i].x, temoins_2d[i].z);
        if (fabsf(res - temoins_2d[i].attendu) > EPSILON) {
            PRINT_BOTH("  [ÉCHEC] 2D au point (X: %f, Z: %f) : obtenu %f, attendu %f\n", 
                       temoins_2d[i].x, temoins_2d[i].z, res, temoins_2d[i].attendu);
            erreurs++;
        }
    }

    /* Validation Bruit 3D */
    for (int i = 0; i < 3; i++) {
        float res = zyn_noise3d(temoins_3d[i].x, temoins_3d[i].z, temoins_3d[i].y);
        if (fabsf(res - temoins_3d[i].attendu) > EPSILON) {
            PRINT_BOTH("  [ÉCHEC] 3D au point (X: %f, Z: %f, Y: %f) : obtenu %f, attendu %f (Changement d'axes)\n", 
                       temoins_3d[i].x, temoins_3d[i].z, temoins_3d[i].y, res, temoins_3d[i].attendu);
            erreurs++;
        }
    }

    if (erreurs == 0) {
        PRINT_BOTH("  [SUCCÈS] Tous les tests de non-régression sont au vert. Le moteur est stable.\n\n");
    } else {
        PRINT_BOTH("  [ALERTE] %d anomalie(s) détectée(s). Si le moteur vient d'être modifié (axes), mettez à jour les témoins.\n\n", erreurs);
    }

    /* =========================================================================
     * PHASE 2 : PROFILAGE ET PERFORMANCE (BENCHMARK)
     * ========================================================================= */
    PRINT_BOTH("[2/2] Lancement du benchmark de stress-test (%d itérations)...\n", NB_ITERATIONS_BENCH);
    
    volatile float accumulation = 0.0f;
    clock_t start, end;
    double temps_cpu;

    /* --- Benchmark 2D --- */
    printf("  Stressing zyn_noise2d... ");
    fflush(stdout);
    fprintf(report, "  Stressing zyn_noise2d... ");
    
    start = clock();
    for (int32_t i = 0; i < NB_ITERATIONS_BENCH; i++) {
        accumulation += zyn_noise2d((float)i * 0.001f, (float)i * 0.0015f);
    }
    end = clock();
    
    temps_cpu = ((double)(end - start)) / CLOCKS_PER_SEC;
    PRINT_BOTH("Terminé en %.4f secondes.\n", temps_cpu);
    PRINT_BOTH("  -> Vitesse 2D : %.2f Millions d'appels/sec\n\n", (NB_ITERATIONS_BENCH / 1000000.0) / temps_cpu);

    /* --- Benchmark 3D --- */
    printf("  Stressing zyn_noise3d... ");
    fflush(stdout);
    fprintf(report, "  Stressing zyn_noise3d... ");
    
    start = clock();
    for (int32_t i = 0; i < NB_ITERATIONS_BENCH; i++) {
        accumulation += zyn_noise3d((float)i * 0.001f, (float)i * 0.0012f, (float)i * 0.0015f);
    }
    end = clock();
    
    temps_cpu = ((double)(end - start)) / CLOCKS_PER_SEC;
    PRINT_BOTH("Terminé en %.4f secondes.\n", temps_cpu);
    PRINT_BOTH("  -> Vitesse 3D : %.2f Millions d'appels/sec\n\n", (NB_ITERATIONS_BENCH / 1000000.0) / temps_cpu);

    if (accumulation == 999999.0f) { printf("%f", accumulation); }

    PRINT_BOTH("=====================================================================\n");
    PRINT_BOTH(" Fin du profilage de l'interface mathématique.\n");
    PRINT_BOTH("=====================================================================\n");

    // Fermeture propre du descripteur de fichier
    fclose(report);
    #undef PRINT_BOTH

    return (erreurs == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
