/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 06/06/2026
 *
 * zyn_map_generator  :
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
#include "zyn_macro_chunk_injector.h"
#include "zyn_gen_map_macro.h"

int main(void) {
    printf("[INFO] Initialisation de zyn_map_generator...\n");

    // 1. Détermination d'une seed aléatoire de départ
    srand((unsigned int)time(NULL));
    uint32_t world_seed = ((uint32_t)rand() << 16) | (uint32_t)rand();
    printf("[INFO] Graine du monde générée (Seed) : %lu\n", (unsigned long)world_seed);

    // 2. Allocation dynamique de la Map (2 millions de macro-chunks)
    printf("[INFO] Allocation de la mémoire pour %d macro-chunks...\n", ZYN_TOTAL_MACRO_CHUNKS);
    MacroChunk* world_map = (MacroChunk*)calloc(ZYN_TOTAL_MACRO_CHUNKS, sizeof(MacroChunk));
    if (!world_map) {
        fprintf(stderr, "[CRITICAL] Mémoire insuffisante pour allouer la map mondiale.\n");
        return EXIT_FAILURE;
    }
    ZynRiverNode*   flux_grid;
    int32_t flux_count;
    // 3. Boucle géométrique de génération procédurale
    printf("[INFO] Génération mathématique du monde en cours...\n");
    zyn_gen_map_macro(world_map,world_seed,&flux_grid,&flux_count, NULL);
    printf("[INFO] Génération procédurale terminée avec succès.\n");

    // 4. Enregistrement en masse (Bulk Insert) dans la base de données
    printf("[INFO] Injection de masse dans SQLite (Bulk Insert)... Cet étape cible < 10s.\n");
    int db_status_relief = zyn_inject_macro_chunks(world_map, ZYN_TOTAL_MACRO_CHUNKS);
    int db_status_river = zyn_inject_macro_river(flux_grid, flux_count);
    
    // 5. Nettoyage strict de la RAM (Zéro fuite)
    free(world_map);
    free(flux_grid);
    if (db_status_relief != 0) {
        fprintf(stderr, "[ERROR] L'enregistrement de la carte relief a échoué (Code: %d).\n", db_status_relief);
        return EXIT_FAILURE;
    }
    if (db_status_river != 0) {
        fprintf(stderr, "[ERROR] L'enregistrement de la carte relief a échoué (Code: %d).\n", db_status_river);
        return EXIT_FAILURE;
    }

    printf("[SUCCESS] La carte mondiale a été générée et persistée avec succès.\n");
    return EXIT_SUCCESS;
}
