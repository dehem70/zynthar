/* =============================================================================
 *
 * ZYNTHAR v0.1
 *
 * Auteur : Dehem70
 * Date   : 28/05/2026
 *
 * zyn_gen_map_macro : Intégration global
 * pour l'intégralité de la chaîne géomorphologique macro (Relief, Climat, Image).
 * Sauvegarde un rapport persistant dans /reports/benchmarks/
 *
 * =============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

#include <zynthar.h>
#include "zyn_noise.h"
#include "zyn_gen_map_relief.h"
#include "zyn_gen_png.h"
#include "zyn_gen_map_temperature.h"
#include "zyn_gen_map_humidity.h"
#include "zyn_gen_map_river.h"
#include "zyn_gen_map_biome.h"

#define SEED_MONDE           7777U
#define ZYN_EPSILON          1e-5f

/**
 * @brief Structure enrichie pour le contrôle de non-régression du monde macro.
 */
typedef struct {
    int32_t x;
    int32_t z;
    float alt_attendue;
    float temp_attendue;
} TemoinMonde;

int main(void) {
    // 1. Initialisation de la tuyauterie des rapports
    char *root_env = getenv("ZYNTHAR_ROOT");
    if (root_env == NULL) {
        fprintf(stderr, "[-] Erreur : La variable d'environnement ZYNTHAR_ROOT n'est pas définie.\n");
        return EXIT_FAILURE;
    }

    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

    char report_path[1024];
    snprintf(report_path, sizeof(report_path), "%s/reports/benchmarks/zyn_test_gen_map_relief_%s.txt", root_env, timestamp);

    FILE *report = fopen(report_path, "w");
    if (report == NULL) {
        fprintf(stderr, "[-] Erreur : Impossible de créer le fichier de benchmark dans /reports/benchmarks/\n");
        return EXIT_FAILURE;
    }

    #define PRINT_BOTH(...) do { printf(__VA_ARGS__); fprintf(report, __VA_ARGS__); } while(0)

    PRINT_BOTH("=====================================================================\n");
    PRINT_BOTH("     ZYNTHAR : PIPELINE GLOBAL DE GÉNÉRATION MACRO-GEOMORPHOLOGIQUE  \n");
    PRINT_BOTH("=====================================================================\n");
    PRINT_BOTH(" Fichier généré : zyn_test_gen_map_relief_%s.txt\n", timestamp);
    PRINT_BOTH(" Rapport d'accès: %s\n", report_path);
    PRINT_BOTH("=====================================================================\n\n");

    clock_t start_global, end_global;
    clock_t start_etape, end_etape;
    double temps_cpu;

    start_global = clock();

    // Alignement géométrique transversal (X) et longitudinal (Z)
    int32_t width_x = ZYN_WORLD_X_MAX / ZYN_MACRO_CHUNK_DIM_M;
    int32_t depth_z = ZYN_WORLD_Z_MAX / ZYN_MACRO_CHUNK_DIM_M;

    /* =========================================================================
     * PHASE 1 : ALLOCATION MÉMOIRE CONTIGUË
     * ========================================================================= */
    PRINT_BOTH("[1/5] Allocation de la grille MacroChunks (%dx%d)... ", width_x, depth_z);
    fflush(stdout);
    
    start_etape = clock();
    MacroChunk* map = zyn_gen_map_relief_alloc(width_x, depth_z);
    end_etape = clock();
    
    if (map == NULL) {
        fclose(report);
        return EXIT_FAILURE;
    }
    PRINT_BOTH("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);

    /* =========================================================================
     * PHASE 2 : FUSION GÉOMORPHOLOGIQUE (VORONOI VECTORISÉ + FRACTAL 2D)
     * ========================================================================= */
    PRINT_BOTH("[2/5] Calcul de l'archipel & calibration mer (4 îles, 45%% eau)... ");
    fflush(stdout);

    zyn_noise_init(SEED_MONDE);

    start_etape = clock();
    zyn_gen_map_relief_archipelago(map, width_x, depth_z, 4, 0.45f);
    end_etape = clock();
    
    PRINT_BOTH("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);

    /* =========================================================================
     * PHASE 3 : AUTOMATE CELLULAIRE DE MOORE DÉROULÉ (LISSAGE DES CÔTES)
     * ========================================================================= */
    PRINT_BOTH("[3/5] Lissage des lignes de côtes par AC (3 itérations)... ");
    fflush(stdout);

    start_etape = clock();
    zyn_gen_map_relief_smooth_coastlines(map, width_x, depth_z, 3);
    end_etape = clock();

    PRINT_BOTH("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);

    /* =========================================================================
     * PHASE 4 : CALCUL MODÈLE THERMIQUE CLIMATIQUE
     * ========================================================================= */
    PRINT_BOTH("[4/5] Application du gradient thermique longitudinal et altitude... ");
    fflush(stdout);

    /* SÉCURITÉ ARCHITECTURALE : On ré-initialise le moteur de bruit avec la graine mondiale
     * juste avant le calcul pour écraser tout effet de bord d'un sous-test précédent */
    zyn_noise_init(SEED_MONDE);
    
    start_etape = clock();
    zyn_gen_map_temperature(map, width_x, depth_z);
    end_etape = clock();

    PRINT_BOTH("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);
    
    /* =========================================================================
     * PHASE 4.5 : CALCUL MODÈLE HYDRO-ATMOSPHÉRIQUE (HUMIDITÉ / PLUIE)
     * ========================================================================= */
    PRINT_BOTH("[4.5/5] Application du transport d'humidité et effet de fœhn... ");
    fflush(stdout);

    start_etape = clock();
    zyn_gen_map_humidity(map, width_x, depth_z);
    end_etape = clock();

    PRINT_BOTH("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);
    
    /* =========================================================================
     * PHASE 4.8 : CALCUL MODÈLE HYDROGRAPHIQUE MACRO CHUNK
     * ========================================================================= */
    PRINT_BOTH("[4.8/5] Tracé du réseau de rivières macro et détection des bassins... ");
    fflush(stdout);

    /* Allocation d'une grille de taille Macro (width_x * depth_z) */
    size_t total_chunks_monde = (size_t)width_x * (size_t)depth_z;
    uint32_t* flux_grid = (uint32_t*)calloc(total_chunks_monde, sizeof(uint32_t));

    start_etape = clock();
    zyn_gen_map_river(map, width_x, depth_z, flux_grid);
    end_etape = clock();
    PRINT_BOTH("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);
    
    /* =========================================================================
     * PHASE 5/5 : ATTRIBUTION LOGIQUE DES BIOMES ET EXPORT CHROMATIQUE
     * ========================================================================= */
    PRINT_BOTH("\n[5/5] Évaluation de la matrice de Whittaker et classification... ");
    fflush(stdout);

    start_etape = clock();
    zyn_gen_map_biome(map, width_x, depth_z);
    end_etape = clock();
    PRINT_BOTH("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);

    /* =========================================================================
     * EXPORTATION DES RÉSULTATS VISUELS SUR DISQUE
     * ========================================================================= */
    PRINT_BOTH("\n[EXPORT] Enregistrement des images PNG d'audit macro... \n");
    
    if (zyn_gen_png_elevation(map, width_x, depth_z, "carte_relief.png","carte_relief_bin.png")) {
        PRINT_BOTH("  -> 'carte_relief.png' exportée avec succès.\n");
    }
    if (zyn_gen_png_temperature(map, width_x, depth_z, "carte_temperature.png")) {
        PRINT_BOTH("  -> 'carte_temperature.png' exportée avec succès.\n");
    }
    if (zyn_gen_png_rivers(map, width_x, depth_z, flux_grid, "carte_rivieres.png")) {
        PRINT_BOTH("  -> 'carte_rivieres.png' (Fleuves & Lacs) exportée avec succès.\n");
    }
    if (zyn_gen_png_biomes(map, width_x, depth_z, "carte_biomes.png")) {
        PRINT_BOTH("  -> 'carte_biomes.png' COULEURS RGB exportée avec succès !\n");
    }

    /* Libération de la grille hydrographique */
    free(flux_grid);

    end_global = clock();
    double temps_total = ((double)(end_global - start_global)) / CLOCKS_PER_SEC;
    PRINT_BOTH("\n=====================================================================\n");
    PRINT_BOTH(" GÉNERATION MACRO TERMINÉE AVEC SUCCÈS EN %.4f SECONDES\n", temps_total);
    PRINT_BOTH("=====================================================================\n");

    free(map);
    return 0;
}
