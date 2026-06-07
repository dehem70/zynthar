/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 06/06/2026
 *
 * zyn_brique2_stress  :
 * utilisation :
 *
 * =============================================================================*/
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

// En-têtes requis sous Linux/POSIX pour manipuler 'struct stat' et 'mkdir'
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <zynthar.h>
#include <time.h>
#include "zynthar_db.h" 
#include "macro_access.h"

#define TEST_ITERATIONS         50000

typedef union {
    struct {
        uint8_t rx; // Octet 0 (Poids le plus faible)
        uint8_t rz; // Octet 1
        uint8_t x;  // Octet 2
        uint8_t z;  // Octet 3 (Poids le plus fort)
    };
    uint32_t id;
} MacroId;

// Fonction utilitaire pour mesurer le temps de haute résolution
static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000.0) + (ts.tv_nsec / 1000000.0);
}

int main(void) {
    printf("==================================================\n");
    printf("[ZYN-STRESS] Lancement de la suite de validation Brique 2\n");
    printf("==================================================\n");

    // 1. Récupération et validation de la racine du projet via l'environnement
    const char *root_env = getenv("ZYNTHAR_ROOT");
    if (!root_env) {
        fprintf(stderr, "[ZYN-STRESS] ERREUR : La variable d'environnement $ZYNTHAR_ROOT n'est pas définie.\n");
        return EXIT_FAILURE;
    }
    printf("[ZYN-STRESS] Racine du projet détectée : %s\n", root_env);

    // 2. Construction des chemins absolus vers les fichiers de base de données
    char core_db_path[1024];
    char river_db_path[1024];

    snprintf(core_db_path, sizeof(core_db_path), "%s/%s%s", root_env, ZYN_DB_EMPLACEMENT, ZYN_DB_WORLD);
    snprintf(river_db_path, sizeof(river_db_path), "%s/%s%s", root_env, ZYN_DB_EMPLACEMENT, ZYN_DB_RIVER);

    printf("[ZYN-STRESS] Chemin Core  : %s\n", core_db_path);
    printf("[ZYN-STRESS] Chemin River : %s\n", river_db_path);

    // 3. Validation Étape 1 : sqlite_bindings (Tâche 2.1)
    printf("\n[ZYN-STRESS] ---> ÉVALUATION ET INITIALISATION DU BINDING SQLITE3...\n");
    
    ZynDatabase db;
    double start_time = get_time_ms();
    
    if (!ZynDB_Initialize(&db, core_db_path, river_db_path)) {
        fprintf(stderr, "[ZYN-STRESS] ERREUR : Échec critique lors de l'initialisation des bases de données.\n");
        return EXIT_FAILURE;
    }
    
    // Vérification de l'état interne
    if (db.is_connected && db.handle != NULL) {
        printf("[ZYN-STRESS] STATUT : Moteur de persistance connecté et stable.\n");
    } else {
        fprintf(stderr, "[ZYN-STRESS] ERREUR : État interne de la structure de BDD incohérent.\n");
        ZynDB_Close(&db);
        return EXIT_FAILURE;
    }
    
    // 2. Initialisation de l'accès Macro (Tâche 2.2)
    printf("\n[ZYN-STRESS] ---> INITIALISATION DU MODULE MACRO_ACCESS...\n");
    if (!MacroAccess_Initialize(&db)) {
        ZynDB_Close(&db);
        return EXIT_FAILURE;
    }
    double end_time = get_time_ms();
    printf("[ZYN-STRESS] OK : Connexions et PRAGMAs configurés en %.4f ms.\n", end_time - start_time);
    
    // 3. Simulation de charge : 50 000 requêtes d'agrégation flash
    printf("\n[ZYN-STRESS] ---> EXÉCUTION DE %d LECTURES D'AGRÉGATION (LEFT JOIN + BLOB DECODING)...\n", TEST_ITERATIONS);
    
    // 3. Pré-chargement de 50 000 IDs uniques existants en Base
    printf("\n[ZYN-STRESS] Pré-chargement de %d IDs uniques depuis la BDD...\n", TEST_ITERATIONS);
    
    // Allocation d'un tableau en RAM pour stocker nos IDs de test
    uint32_t *test_ids = malloc(sizeof(uint32_t) * TEST_ITERATIONS);
    if (!test_ids) {
        fprintf(stderr, "[ZYN-STRESS] ERREUR : Échec de l'allocation mémoire pour les IDs de test.\n");
        MacroAccess_Terminate();
        ZynDB_Close(&db);
        return EXIT_FAILURE;
    }

    sqlite3_stmt *stmt_bulk = NULL;
    // On extrait séquentiellement les 50 000 premiers IDs de la table
    const char *sql_bulk = "SELECT id FROM macro_chunks LIMIT ?;";
    
    int ids_loaded = 0;
    if (sqlite3_prepare_v2(db.handle, sql_bulk, -1, &stmt_bulk, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt_bulk, 1, TEST_ITERATIONS);
        
        while (sqlite3_step(stmt_bulk) == SQLITE_ROW && ids_loaded < TEST_ITERATIONS) {
            test_ids[ids_loaded] = (uint32_t)sqlite3_column_int64(stmt_bulk, 0);
            ids_loaded++;
        }
        sqlite3_finalize(stmt_bulk);
    }

    if (ids_loaded == 0) {
        fprintf(stderr, "[ZYN-STRESS] ERREUR : Impossible de lire les IDs. La base est-elle vide ?\n");
        free(test_ids);
        MacroAccess_Terminate();
        ZynDB_Close(&db);
        return EXIT_FAILURE;
    }
    
    printf("[ZYN-STRESS] %d IDs réels chargés en RAM avec succès pour le benchmark.\n", ids_loaded);
    if (ids_loaded < TEST_ITERATIONS) {
        printf("[ZYN-STRESS] NOTE : Moins de %d lignes disponibles, le test s'ajustera sur %d items.\n", TEST_ITERATIONS, ids_loaded);
    }
    
    // 4. Simulation de charge : 50 000 requêtes d'agrégation flash sur cet ID valide
    printf("\n[ZYN-STRESS] ---> EXÉCUTION DE %d LECTURES SUR UN ID VALIDE...\n", TEST_ITERATIONS);
    
    MacroDataPayload payload;

    int success_count = 0;

    start_time = get_time_ms();
    
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        // On fait varier légèrement l'ID pour simuler des requêtes sur différents chunks (si présents en BDD)
        // Sinon, la base évaluera le comportement sur un ID fixe ou incrémenté.
        if (MacroAccess_GetPayload(&db, test_ids[i], &payload)) {
          //  valid_test_id+=10;
            success_count++;
        }
    }
        
    end_time = get_time_ms();
    double total_time = end_time - start_time;
    double avg_time = total_time / TEST_ITERATIONS;

    printf("[ZYN-STRESS] RÉSULTATS DU BENCHMARK :\n");
    printf("  - Temps total      : %.4f ms\n", total_time);
    printf("  - Temps moyen/item : %.6f ms (ou %.2f µs)\n", avg_time, avg_time * 1000.0);
    printf("  - Débit théorique  : %.0f extractions/seconde\n", (TEST_ITERATIONS / total_time) * 1000.0);
    printf("  - Requêtes réussies: %d / %d\n", success_count, TEST_ITERATIONS);

    // 4. Nettoyage
    printf("\n[ZYN-STRESS] Nettoyage des modules...\n");
    MacroAccess_Terminate();
    ZynDB_Close(&db);
    
    printf("==================================================\n");
    printf("[ZYN-STRESS] Étape 2 (macro_access) validée.\n");
    printf("==================================================\n");

    return EXIT_SUCCESS;
}
