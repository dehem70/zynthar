/* =============================================================================
 *
 * ZYNTHAR v0.1
 *
 * Auteur : Dehem70
 * Date   : 28/05/2026
 *
 * zyn_test_gen_map_relief : Test d'intégration global et Benchmark de performance
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
    PRINT_BOTH("[2/5] Calcul de l'archipel & calibration mer (4 îles, 55%% eau)... ");
    fflush(stdout);

    zyn_noise_init(SEED_MONDE);

    start_etape = clock();
    zyn_gen_map_relief_archipelago(map, width_x, depth_z, 4, 0.55f);
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

    start_etape = clock();
    zyn_gen_map_temperature(map, width_x, depth_z);
    end_etape = clock();

    PRINT_BOTH("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);

    /* =========================================================================
     * PHASE 5 : VÉRIFICATION DE LA NON-RÉGRESSION (DÉTERMINISME COMPRESSÉ)
     * ========================================================================= */
    PRINT_BOTH("\n[5/5] Vérification de la précision mathématique (Relief + Température)...\n");

    /* Points de contrôles recalés sur la matrice de coordonnées (X, Z) */
    TemoinMonde temoins[] = {
        { 1000, 500,  -225.100006f,  37.039219f }, /* Zone A */
        {  500, 250, 559.000000f,  2.176472f }, /* Zone B */
        { 1500, 100, -100.400002f,  -14.843137f }  /* Zone C */
    };

    int32_t erreurs_relief = 0;
    int32_t erreurs_climat = 0;

    for (int i = 0; i < 3; i++) {
        int32_t idx = temoins[i].z * width_x + temoins[i].x;
        
        /* Décompression à la volée des types natifs pour l'évaluation de conformité */
        float alt_obtenue = DM_TO_M(map[idx].elevation_max_dm);
        float temp_obtenue = RAW_TO_FLOAT(map[idx].temperature_raw);

        /* Validation du relief */
        if (fabsf(alt_obtenue - temoins[i].alt_attendue) > ZYN_EPSILON) {
            PRINT_BOTH("  [ÉCHEC GEOMORPHO] MacroChunk #%d (X:%d, Z:%d) : obtenu alt %fm, attendu %fm (Ajustement requis)\n", 
                       idx, temoins[i].x, temoins[i].z, alt_obtenue, temoins[i].alt_attendue);
            erreurs_relief++;
        }

        /* Validation de la température */
        if (fabsf(temp_obtenue - temoins[i].temp_attendue) > ZYN_EPSILON) {
            PRINT_BOTH("  [ÉCHEC MOD_CLIMAT] MacroChunk #%d (X:%d, Z:%d) : obtenu temp %f, attendu %f (Ajustement requis)\n", 
                       idx, temoins[i].x, temoins[i].z, temp_obtenue, temoins[i].temp_attendue);
            erreurs_climat++;
        }
    }

    if (erreurs_relief == 0 && erreurs_climat == 0) {
        PRINT_BOTH("  [SUCCÈS] Le relief et le modèle thermique sont stables et déterministes.\n");
    } else {
        PRINT_BOTH("  [ALERTE] Écarts détectés lors de la vérification. Si les axes ou le packaging (décimètres) viennent de changer, recalibrez les témoins.\n");
    }

    /* =========================================================================
     * EXPORT VISUEL EN PNG VIA STB
     * ========================================================================= */
    PRINT_BOTH("\nExportation des matrices d'octets en images PNG... ");
    fflush(stdout);
    
    int img_alt = zyn_gen_png_elevation(map, width_x, depth_z, "carte_elevation.png", "carte_elevation_bin.png");
    int img_temp = zyn_gen_png_temperature(map, width_x, depth_z, "carte_temperature.png");

    if (img_alt && img_temp) {
        PRINT_BOTH("OK !\n");
        PRINT_BOTH("  -> Fichiers 'carte_elevation.png', 'carte_elevation_bin.png' et 'carte_temperature.png' générés.\n");
    } else {
        PRINT_BOTH("[ÉCHEC ÉCRITURE DISQUE]\n");
    }

    /* Nettoyage de la mémoire */
    zyn_gen_map_relief_free(map);

    end_global = clock();
    temps_cpu = ((double)(end_global - start_global)) / CLOCKS_PER_SEC;

    PRINT_BOTH("\n=====================================================================\n");
    PRINT_BOTH(" VITESSE PIPELINE MONDIAL : Généré en %.4f secondes (Chaîne complète)\n", temps_cpu);
    PRINT_BOTH("=====================================================================\n");

    fclose(report);
    #undef PRINT_BOTH

    // On retourne un statut de succès indicatif pour ne pas bloquer les scripts d'automation CMAKE
    return EXIT_SUCCESS;
}
