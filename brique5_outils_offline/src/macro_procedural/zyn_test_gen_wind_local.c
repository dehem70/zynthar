/* =============================================================================
 *
 * ZYNTHAR v0.1
 *
 * src/macro_procedural/zyn_test_gen_wind_local.c
 * Validation fonctionnelle et stress-test de performance du vent local.
 *
 * =============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <zynthar.h>
#include "zyn_noise.h"
#include "zyn_gen_map_relief.h"
#include "zyn_gen_wind_local.h"
#include "zyn_gen_wind_global.h"

#define NB_ITERATIONS_BENCH  10000000 /* 10 Millions d'itérations */
#define SEED_MONDE           7777U

int main(void) {
    char *root_env = getenv("ZYNTHAR_ROOT");
    if (root_env == NULL) {
        fprintf(stderr, "[-] Erreur : La variable ZYNTHAR_ROOT n'est pas définie.\n");
        return EXIT_FAILURE;
    }

    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

    char report_path[1024];
    snprintf(report_path, sizeof(report_path), "%s/reports/benchmarks/zyn_test_gen_wind_local_%s.txt", root_env, timestamp);

    FILE *report = fopen(report_path, "w");
    if (report == NULL) {
        fprintf(stderr, "[-] Erreur : Création du rapport impossible dans /reports/benchmarks/\n");
        return EXIT_FAILURE;
    }

    #define PRINT_BOTH(...) do { printf(__VA_ARGS__); fprintf(report, __VA_ARGS__); } while(0)

    PRINT_BOTH("=====================================================================\n");
    PRINT_BOTH("       ZYNTHAR : VALIDATION & STRESS-TEST DU VENT LOCAL MACRO        \n");
    PRINT_BOTH("=====================================================================\n");
    PRINT_BOTH(" Horodatage : %s\n", timestamp);
    PRINT_BOTH(" Rapport    : %s\n", report_path);
    PRINT_BOTH("=====================================================================\n\n");

    /* Initialisation de l'environnement factice de test (Grille mondiale) */
    int32_t width_x = ZYN_WORLD_X_MAX / ZYN_MACRO_CHUNK_DIM_M;
    int32_t depth_z = ZYN_WORLD_Z_MAX / ZYN_MACRO_CHUNK_DIM_M;

    PRINT_BOTH("[1/3] Génération d'un relief de référence pour le test...\n");
    MacroChunk* map = zyn_gen_map_relief_alloc(width_x, depth_z);
    assert(map != NULL);
    
    zyn_noise_init(SEED_MONDE);
    zyn_gen_map_relief_archipelago(map, width_x, depth_z, 4, 0.55f);

    int32_t tx = 500;
    int32_t tz = 250;
    int32_t erreurs = 0;

    /* =========================================================================
     * PHASE 1 : VALIDATION DU DÉTERMINISME ET DE LA PHYSIQUE
     * ========================================================================= */
    PRINT_BOTH("[2/3] Validation de la stabilité fonctionnelle au point (%d, %d)...\n", tx, tz);
    
    WindVector wl1 = zyn_gen_map_wind_local(map, width_x, depth_z, tx, tz);
    WindVector wl2 = zyn_gen_map_wind_local(map, width_x, depth_z, tx, tz);

    PRINT_BOTH("  -> Vecteur Local 1 : dx = %.4f, dy = %.4f\n", wl1.dx, wl1.dy);
    PRINT_BOTH("  -> Vecteur Local 2 : dx = %.4f, dy = %.4f\n", wl2.dx, wl2.dy);

    if (wl1.dx != wl2.dx || wl1.dy != wl2.dy) {
        PRINT_BOTH("  [ÉCHEC] Rupture du déterminisme strict sur le vent local !\n");
        erreurs++;
    }

    if (isnan(wl1.dx) || isnan(wl1.dy)) {
        PRINT_BOTH("  [ÉCHEC] Valeur non numérique (NaN) détectée.\n");
        erreurs++;
    }

    float v_scal = sqrtf(wl1.dx * wl1.dx + wl1.dy * wl1.dy);
    PRINT_BOTH("  -> Vitesse scalaire physique locale : %.2f km/h (Seuil max : 140.00 km/h)\n", v_scal);

    if (v_scal > 140.0001f) {
        PRINT_BOTH("  [ÉCHEC] Dépassement du garde-fou de vitesse physique de l'univers.\n");
        erreurs++;
    }

    if (erreurs == 0) {
        PRINT_BOTH("  [SUCCÈS] Validation fonctionnelle accomplie avec stabilité.\n\n");
    }

    /* =========================================================================
     * PHASE 2 : PROFILAGE DE PERFORMANCE ET STRESS-TEST (COMPLEXITÉ O(1))
     * ========================================================================= */
    PRINT_BOTH("[3/3] Lancement du benchmark de performance (%d itérations)... \n", NB_ITERATIONS_BENCH);
    printf("  Stressing zyn_gen_map_wind_local... ");
    fflush(stdout);
    fprintf(report, "  Stressing zyn_gen_map_wind_local... ");

    volatile float acc_x = 0.0f;
    volatile float acc_y = 0.0f;

    clock_t start = clock();
    for (int32_t i = 0; i < NB_ITERATIONS_BENCH; i++) {
        /* On fait varier dynamiquement les coordonnées pour forcer les sauts de cache L1/L2 de la grille map */
        int32_t dynamic_x = i % width_x;
        int32_t dynamic_z = (i / width_x) % depth_z;

        WindVector w_bench = zyn_gen_map_wind_local(map, width_x, depth_z, dynamic_x, dynamic_z);
        acc_x += w_bench.dx;
        acc_y += w_bench.dy;
    }
    clock_t end = clock();

    double temps_cpu = ((double)(end - start)) / CLOCKS_PER_SEC;
    PRINT_BOTH("Terminé en %.4f secondes.\n", temps_cpu);
    PRINT_BOTH("  -> Performance : %.2f Millions d'échantillonnages locaux/sec\n", (NB_ITERATIONS_BENCH / 1000000.0) / temps_cpu);
    PRINT_BOTH("=====================================================================\n");

    /* Libération propre */
    zyn_gen_map_relief_free(map);
    fclose(report);
    #undef PRINT_BOTH

    /* Masquage de l'optimisation agressive du compilateur pour les accumulateurs */
    if (acc_x == 1.234f) printf("%f", acc_y);

    return (erreurs == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
