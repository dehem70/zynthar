/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 14/06/2026
 *
 * zyn_stress_math  :
 * utilisation :
 *
 * =============================================================================*/
  
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "zyn_math.h"

#define NB_ITERATIONS 100000000 // 100 Millions d'itérations pour saturer le CPU

int main(void) {
    // Configuration des tampons de sortie
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("========================================================================\n");
    printf("[🧪 ZYN_MATH] Lancement du Stress Test fonction interpolation linéraire\n");
    printf("========================================================================\n");

    // 1. Définition d'un cadre fixe réaliste pour le test (Ex: un Macro-Chunk de 512m)
    uint32_t d = 5120; // 5120 dm

    // Altitudes des 4 coins (valeurs réalistes entre vallées et montagnes en dm)
    int32_t h_nw = 1200;  // 120m
    int32_t h_ne = 3450;  // 345m
    int32_t h_sw = -450;  // -45m (sous le niveau 0)
    int32_t h_se = 5800;  // 580m

    // Initialisation du générateur pseudo-aléatoire pour simuler des positions de joueurs
    srand(42);

    printf("[⏳] Génération et calcul de %d millions d'interpolations...\n", NB_ITERATIONS / 1000000);

    // Variables pour la mesure de temps (haute résolution)
    struct timespec start_time, end_time;
    double cumul_time;
    
    cumul_time= 0;
    
    // Accumulateur témoin pour empêcher le compilateur d'optimiser en supprimant la boucle (-O3)
    int64_t dummy_checksum = 0;

    // 2. Boucle de Stress Test intensive
    for (uint32_t i = 0; i < NB_ITERATIONS; i++) {
        // Simulation de coordonnées x et z contenues strictement dans le carré [0, d]
        uint32_t x = (uint32_t)(rand() % (d + 1));
        uint32_t z = (uint32_t)(rand() % (d + 1));
        
        clock_gettime(CLOCK_MONOTONIC, &start_time);

        // Appel de notre fonction de point fixe
        int32_t res = zyn_math_bilinear_int(d, h_nw, h_ne, h_sw, h_se, x, z);
        
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        
        cumul_time += (double)(end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                           (double)(end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;


        // Injection dans le checksum témoin
        dummy_checksum += res;
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time);

    // 3. Calculs des métriques de performances
    //double total_time_ms = (double)(end_time.tv_sec - start_time.tv_sec) * 1000.0 +
    //                       (double)(end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
    
    double total_time_ms=cumul_time;
    
    double total_time_sec = total_time_ms / 1000.0;
    double mops = ((double)NB_ITERATIONS / total_time_sec) / 1000000.0;
    double latence_ns = (total_time_ms * 1000000.0) / (double)NB_ITERATIONS;

    // 4. Affichage du Bilan du Stress Test
    printf("\n================= BILAN DU STRESS TEST =================\n");
    printf("[🚀] Opérations exécutées    : %d\n", NB_ITERATIONS);
    printf("[⏱️] Durée totale du test     : %.4f ms (soit %.4f secondes)\n", total_time_ms, total_time_sec);
    printf("[📊] Cadence brute (Débit)   : %.2f MOPS (Millions d'Opérations/Sec)\n", mops);
    printf("[💎] Latence moyenne estimée : %.2f nanosecondes par interpolation\n", latence_ns);
    printf("[🧮] Checksum de sécurité    : %lld (Garantie d'exécution intégrale)\n", (long long)dummy_checksum);
    if (dummy_checksum!=250019540110) {
       printf("!!! ALERTE !!! Les résultats ne sont plus les mêmes\n");
    }
    printf("========================================================\n");
    
    printf("========================================================\n");
    printf("[🧪 ZYN_MATH] Lancement du Stress Test BIQUADRATIQUE 2D\n");
    printf("========================================================\n");

    // 1. Paramètres d'entrée fixes (Simulation d'un Macro-Chunk de 512m)

    int32_t world_x = 1024512;   // Position mondiale fictive X (en dm)
    int32_t world_z =  512000;   // Position mondiale fictive Z (en dm)

    int32_t h_centre = 2200;     // 220m

    // Amplitudes spécifiques aux 4 arêtes (gérées par Chronos selon les biomes voisins)
    int32_t amp_n = 20;          // Bord Nord plutôt calme (ex: Plaine)
    int32_t amp_s = 250;         // Bord Sud très accidenté (ex: Montagne)
    int32_t amp_w = 0;           // Bord Ouest plat (ex: Océan / Shunt)
    int32_t amp_e = 120;         // Bord Est modéré (ex: Collines)


    printf("[⏳] 1. Forge de la grille fractale 3x3...\n");
    ZynQuadraticGrid grid = zyn_math_forge_fractal_grid(
        h_nw, h_ne, h_sw, h_se, h_centre,
        world_x, world_z, d,
        amp_n, amp_s, amp_w, amp_e
    );

    // Petit affichage de contrôle pour vérifier l'état de la forge
    printf("     -> Points cardinaux ondulés générés :\n");
    printf("        [N]: %d dm | [S]: %d dm | [W]: %d dm | [E]: %d dm\n", 
           grid.n, grid.s, grid.w, grid.e);

    printf("[⏳] 2. Exécution de %d millions d'interpolations biquadratiques...\n", NB_ITERATIONS / 1000000);

    cumul_time= 0;
    
    // Accumulateur témoin pour empêcher le compilateur d'optimiser en supprimant la boucle (-O3)
    dummy_checksum = 0;
    // Boucle intensive de stress mathématique
    for (uint32_t i = 0; i < NB_ITERATIONS; i++) {
        // Coordonnées cibles aléatoires mais strictement confinées dans le carré [0, d]
        uint32_t x = (uint32_t)(rand() % (d + 1));
        uint32_t z = (uint32_t)(rand() % (d + 1));
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        // Appel de la fonction biquadratique 2D
        int32_t res = zyn_math_biquadratic_int(grid, x, z);
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        
        cumul_time += (double)(end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                           (double)(end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;

        dummy_checksum += res;
    }


    // Calcul des métriques de performance
    total_time_ms = cumul_time;
    
    total_time_sec = total_time_ms / 1000.0;
    mops = ((double)NB_ITERATIONS / total_time_sec) / 1000000.0;
    latence_ns = (total_time_ms * 1000000.0) / (double)NB_ITERATIONS;

    printf("\n================= BILAN DU STRESS TEST =================\n");
    printf("[🚀] Opérations exécutées    : %d\n", NB_ITERATIONS);
    printf("[⏱️] Durée totale du test     : %.4f ms (soit %.4f secondes)\n", total_time_ms, total_time_sec);
    printf("[📊] Cadence brute (Débit)   : %.2f MOPS (Millions d'Opérations/Sec)\n", mops);
    printf("[💎] Latence moyenne estimée : %.2f nanosecondes par point 2D\n", latence_ns);
    printf("[🧮] Checksum de déterminisme: %lld (Doit être identique à chaque run)\n", (long long)dummy_checksum);
    if (dummy_checksum!=235802837122) {
       printf("!!! ALERTE !!! Les résultats ne sont plus les mêmes\n");
    }
    printf("========================================================\n");


    return EXIT_SUCCESS;
}
