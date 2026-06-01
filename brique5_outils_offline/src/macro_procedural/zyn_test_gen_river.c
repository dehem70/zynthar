/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 31/05/2026
 *
 * zyn_test_gen_river  : Validation et profilage de charge CPU de la brique hydrographique.
 * utilisation :
 *
 * =============================================================================*/
 
 #include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <assert.h>

#include <zynthar.h>
#include "zyn_noise.h"
#include "zyn_gen_map_relief.h"
#include "zyn_gen_map_temperature.h"
#include "zyn_gen_map_humidity.h"
#include "zyn_gen_map_river.h"

#define SEED_MONDE 7777U

int main(void) {
    char *root_env = getenv("ZYNTHAR_ROOT");
    if (root_env == NULL) return EXIT_FAILURE;

    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

    char report_path[1024];
    snprintf(report_path, sizeof(report_path), "%s/reports/benchmarks/zyn_test_gen_river_%s.txt", root_env, timestamp);

    FILE *report = fopen(report_path, "w");
    if (report == NULL) return EXIT_FAILURE;

    #define PRINT_BOTH(...) do { printf(__VA_ARGS__); fprintf(report, __VA_ARGS__); } while(0)

    PRINT_BOTH("=====================================================================\n");
    PRINT_BOTH("         ZYNTHAR : RAPPORT HYDROGRAPHIQUE (RIVIÈRES MICRO)           \n");
    PRINT_BOTH("=====================================================================\n");
    PRINT_BOTH(" Horodatage : %s\n", timestamp);
    PRINT_BOTH(" Fichier    : %s\n", report_path);
    PRINT_BOTH("=====================================================================\n\n");

    int32_t width_x = ZYN_WORLD_X_MAX / ZYN_MACRO_CHUNK_DIM_M;
    int32_t depth_z = ZYN_WORLD_Z_MAX / ZYN_MACRO_CHUNK_DIM_M;

    PRINT_BOTH("[1/3] Génération du modèle géomorphologique mondial... ");
    MacroChunk* map = zyn_gen_map_relief_alloc(width_x, depth_z);
    assert(map != NULL);

    zyn_noise_init(SEED_MONDE);
    zyn_gen_map_relief_archipelago(map, width_x, depth_z, 4, 0.55f,SEED_MONDE);
    zyn_gen_map_temperature(map, width_x, depth_z);
    zyn_gen_map_humidity(map, width_x, depth_z);
    PRINT_BOTH("OK.\n");

    size_t total_micro = (size_t)width_x * 20 * (size_t)depth_z * 20;
    uint32_t* flux_grid = (uint32_t*)malloc(total_micro * sizeof(uint32_t));
    assert(flux_grid != NULL);

    /* =========================================================================
     * PHASE 1 : COHÉRENCE ET ANCRAGE
     * ========================================================================= */
    PRINT_BOTH("[2/3] Tracé du réseau hydrographique vectoriel... ");
    zyn_gen_map_river(map, width_x, depth_z, flux_grid);
    PRINT_BOTH("OK.\n");

    /* Comptage des cellules fluviales actives pour le témoin de non-régression */
    size_t cellules_actives = 0;
    uint32_t max_flux_trouve = 0;
    for (size_t i = 0; i < total_micro; i++) {
        if (flux_grid[i] > 0) {
            cellules_actives++;
            if (flux_grid[i] > max_flux_trouve) max_flux_trouve = flux_grid[i];
        }
    }

    PRINT_BOTH("  -> Nombre de lits de rivières fins générés : %zu cellules\n", cellules_actives);
    PRINT_BOTH("  -> Débit maximal enregistré (Fleuve majeur) : %u\n", max_flux_trouve);

    /* =========================================================================
     * PHASE 2 : STRESS-TEST DE SURCHARGE CPU
     * ========================================================================= */
    PRINT_BOTH("\n[3/3] Lancement du stress-test hydrographique (15 passes globales)... \n");
    printf("  Stressing zyn_gen_map_river... ");
    fflush(stdout);
    fprintf(report, "  Stressing zyn_gen_map_river... ");

    clock_t start = clock();
    for (int i = 0; i < 15; i++) {
        zyn_gen_map_river(map, width_x, depth_z, flux_grid);
    }
    clock_t end_etape = clock();

    double temps_cpu = ((double)(end_etape - start)) / CLOCKS_PER_SEC;
    PRINT_BOTH("Terminé en %.4f secondes.\n", temps_cpu);
    PRINT_BOTH("  -> Performance : %.2f passes mondiales hydrographiques complètes/sec\n", 15.0 / temps_cpu);
    PRINT_BOTH("=====================================================================\n");

    free(flux_grid);
    zyn_gen_map_relief_free(map);
    fclose(report);
    #undef PRINT_BOTH

    return EXIT_SUCCESS;
}
