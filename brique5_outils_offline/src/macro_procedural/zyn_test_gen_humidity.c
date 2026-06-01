/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 31/05/2026
 *
 * zyn_test gen_map_humidity  : Validation et Stress-Test de performance du modèle d'humidité.
 * utilisation :
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
#include "zyn_gen_map_temperature.h"
#include "zyn_gen_map_humidity.h"

#define NB_ITERATIONS_BENCH  10 /* 10 passes pour le profilage thermique/charge */
#define SEED_MONDE           7777U

/**
 * @brief Structure pour le contrôle de régression hydro-atmosphérique.
 */
typedef struct {
    int32_t x;
    int32_t z;
    uint8_t raw_attendue;
    const char* nom_zone;
} TemoinHumidite;

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
    snprintf(report_path, sizeof(report_path), "%s/reports/benchmarks/zyn_test_gen_humidity_%s.txt", root_env, timestamp);

    FILE *report = fopen(report_path, "w");
    if (report == NULL) return EXIT_FAILURE;

    #define PRINT_BOTH(...) do { printf(__VA_ARGS__); fprintf(report, __VA_ARGS__); } while(0)

    PRINT_BOTH("=====================================================================\n");
    PRINT_BOTH("         ZYNTHAR : RAPPORT DE COHÉRENCE HYDRO-CLIMATIQUE             \n");
    PRINT_BOTH("=====================================================================\n");
    PRINT_BOTH(" Horodatage : %s\n", timestamp);
    PRINT_BOTH(" Fichier    : %s\n", report_path);
    PRINT_BOTH("=====================================================================\n\n");

    /* Dimensions alignées sur la matrice mondiale officielle */
    int32_t width_x = ZYN_WORLD_X_MAX / ZYN_MACRO_CHUNK_DIM_M;
    int32_t depth_z = ZYN_WORLD_Z_MAX / ZYN_MACRO_CHUNK_DIM_M;
    int32_t erreurs = 0;

    PRINT_BOTH("[1/3] Initialisation des couches géomorphologiques de base... ");
    fflush(stdout);
    MacroChunk* map = zyn_gen_map_relief_alloc(width_x, depth_z);
    assert(map != NULL);

    zyn_noise_init(SEED_MONDE);
    zyn_gen_map_relief_archipelago(map, width_x, depth_z, 4, 0.55f,SEED_MONDE);
    zyn_gen_map_temperature(map, width_x, depth_z);
    PRINT_BOTH("OK.\n");

    /* =========================================================================
     * PHASE 1 : VALIDATION DES VALEURS DE RÉFÉRENCE (NON-RÉGRESSION)
     * ========================================================================= */
    PRINT_BOTH("[2/3] Analyse de conformité face aux témoins de référence...\n");
    
    // Premier calcul officiel
    zyn_gen_map_humidity(map, width_x, depth_z);

    /* Valeurs de référence mathématiques fixes produites par l'algorithme orographique */
    TemoinHumidite temoins[] = {
        { 1000, 500, 135, "Zone A (Plaine Équatoriale)" },
        {  500, 250, 69, "Zone B (Sommet Nord / Fœhn)" },
        { 1500, 100,  54, "Zone C (Bassin Polaire Aride)" }
    };

    for (int i = 0; i < 3; i++) {
        int32_t idx = temoins[i].z * width_x + temoins[i].x;
        uint8_t raw_obtenue = map[idx].biome; /* Stockage temporaire dans l'octet biome */

        if (raw_obtenue != temoins[i].raw_attendue) {
            PRINT_BOTH("  [ÉCHEC] %s au point (%d,%d) : Obtenu %u/255, Attendu %u/255\n",
                       temoins[i].nom_zone, temoins[i].x, temoins[i].z, raw_obtenue, temoins[i].raw_attendue);
            erreurs++;
        } else {
            PRINT_BOTH("  [SUCCÈS] %s : %u/255 (Conforme)\n", temoins[i].nom_zone, raw_obtenue);
        }
    }

    /* =========================================================================
     * PHASE 2 : STRESS-TEST DE CHARGE CPU (COMPLEXITÉ ASSURÉE O(1))
     * ========================================================================= */
    PRINT_BOTH("\n[3/3] Lancement du stress-test de charge CPU...\n");
    printf("  Stressing zyn_gen_map_humidity... ");
    fflush(stdout);
    fprintf(report, "  Stressing zyn_gen_map_humidity... ");

    clock_t start = clock();
    for (int32_t i = 0; i < NB_ITERATIONS_BENCH; i++) {
        zyn_gen_map_humidity(map, width_x, depth_z);
    }
    clock_t end = clock();

    double temps_cpu = ((double)(end - start)) / CLOCKS_PER_SEC;
    double total_cases_calculees = (double)width_x * depth_z * NB_ITERATIONS_BENCH;

    PRINT_BOTH("Terminé en %.4f secondes.\n", temps_cpu);
    PRINT_BOTH("  -> Performance brute : %.2f Millions de chunks/sec\n", (total_cases_calculees / 1000000.0) / temps_cpu);
    PRINT_BOTH("=====================================================================\n");

    zyn_gen_map_relief_free(map);
    fclose(report);
    #undef PRINT_BOTH

    return (erreurs == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
