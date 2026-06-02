/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 01/06/2026
 *
 * zyn_test  : Déroulement des tests de validation
 * utilisation :
 *
 * =============================================================================*/
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <time.h>

// En-têtes requis sous Linux/POSIX pour manipuler 'struct stat' et 'mkdir'
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <zynthar.h>
#include "zyn_test_framework.h"
#include "zyn_gen_map_macro.h"

int main(int argc, char** argv) {

// paramétrage d'un mode fuzz a faire plus tard

    int32_t width = ZYN_WORLD_MACRO_WIDTH_X;
    int32_t depth = ZYN_WORLD_MACRO_DEPTH_Z;
    double total_chunks_par_carte = (double)width * (double)depth;

    printf("==================================================\n");
    printf("   BATTERIE DE TESTS PROCÉDURAUX - ZYNTHAR\n");
    printf("==================================================\n\n");

    /* 1. Étalonnage obligatoire du thermomètre de contrôle */
    if (!zyn_test_calibrate_framework(width, depth)) {
        fprintf(stderr, "[FATAL] Le framework de contrôle est biaisé. Opération annulée.\n");
        return EXIT_FAILURE;
    }

    /* 2. CONFIGURATION DU DRAPEAU DE TEST POUR LE PAS 1 */
    ZynTestConfig config;
    config.active_test = 1;   
    config.target_step = 2;
    config.stress_runs = 20;  
    config.early_exit = 0;
    /* Documentation des target_step
        1 : Génération carte avec voronoi
        2 : injection bruit fractal
        
    */    
    uint32_t base_seeds[20] = {
        12345, 98765, 45612, 89123, 11111, 
        22222, 33333, 44444, 55555, 0xABCDE,
        0x5F375, 77777, 88888, 99999, 124578,
        963258, 741258, 852014, 369258, 159357
    };
    
    uint32_t single_seed = 0;
    int32_t use_single_seed = 0;

    /* ANALYSE DE LA LIGNE DE COMMANDE POUR CHERCHER UN ARGUMENT --seed */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--seed") == 0 && (i + 1) < argc) {
            single_seed = (uint32_t)strtoul(argv[i + 1], NULL, 10);
            use_single_seed = 1;
            config.stress_runs = 1; /* Un seul run demandé si graine unique */
            break;
        }
        /* Option B : Modifier le nombre de runs (seulement si on n'est pas en seed unique) */
        else if (strcmp(argv[i], "--stress") == 0 && (i + 1) < argc && !use_single_seed) {
            config.stress_runs = atoi(argv[i + 1]);
            if (config.stress_runs < 1) config.stress_runs = 1;
        }
    }

    printf("--------------------------------------------------\n");
    if (use_single_seed) {
        printf("[PAS 1] Analyse ciblée sur une graine unique : %u\n", single_seed);
    } else {
        printf("[PAS 1] Lancement du stress-test complet (%d runs)...\n", config.stress_runs);
    }
    printf("Dimensions évaluées : %d x %d Macro-Chunks\n", width, depth);
    printf("--------------------------------------------------\n");

    /* STRUCTURES POUR LE CHRONOMÉTRAGE HAUTE RÉSOLUTION */
    struct timespec start_time, end_time;
    double temps_total_sec = 0.0;

    /* DÉMARRAGE DU CHRONOMÈTRE GLOBAL */
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    if (use_single_seed) {
        /* EXÉCUTION UNITAIRE */
        printf("[RUN UNIQUE] Évaluation de la Seed: %u\n", single_seed);
        zyn_gen_map_macro(single_seed, &config);
    } else {
        /* EXÉCUTION EN BOUCLE */
        for (int32_t i = 0; i < config.stress_runs; i++) {
            uint32_t current_seed = 0;
            config.early_exit = 0;
            
            /* Si on est dans la plage des 20 premières, on prend le tableau fixe */
            if (i < 20) {
                current_seed = base_seeds[i];
            } else {
                /* Au-delà de 20, on dérive les graines de façon déterministe */
                current_seed = base_seeds[i % 20] + (uint32_t)(i / 20) * 7919; 
            }

            printf("\n[RUN %03d/%03d] Évaluation Seed: %u\n", i + 1, config.stress_runs, current_seed);
            zyn_gen_map_macro(current_seed, &config);
        }
    }

    /* ARRÊT DU CHRONOMÈTRE GLOBAL */
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    /* Calcul du temps écoulé en secondes (secondes + nanosecondes converties) */
    temps_total_sec = (double)(end_time.tv_sec - start_time.tv_sec) +
                      (double)(end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    double temps_moyen_par_carte_ms = (temps_total_sec / (double)config.stress_runs) * 1000.0;
    double total_chunks_traites = total_chunks_par_carte * (double)config.stress_runs;
    double chunks_par_seconde = total_chunks_traites / temps_total_sec;

    /* 3. RAPPORT DE NON-RÉGRESSION QUANTITATIF (PERFORMANCE) */
    printf("\n==================================================\n");
    printf("   BILAN DE PERFORMANCE & VITESSE (PAS 1)\n");
    printf("==================================================\n");
    printf("Temps total d'exécution  : %.4f secondes\n", temps_total_sec);
    printf("Temps moyen par carte    : %.2f ms / carte\n", temps_moyen_par_carte_ms);
    printf("Débit du pipeline        : %.0f Macro-Chunks / seconde\n", chunks_par_seconde);
    printf("==================================================\n\n");

    return EXIT_SUCCESS;
}
