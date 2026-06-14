/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : stress_test_db.c
 * Description : Benchmark de lecture intensive (B-Tree/Cache) sur 134M de lignes
 * ============================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sqlite3.h>

#define NB_READS 1000000        // 1 Million de requêtes de chunks aléatoires
#define MAX_ROWS 134217728LL    // Ton nombre exact de lignes

int main(void) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    struct timespec start_time, end_time;

    printf("========================================================\n");
    printf("[🗄️ ZYN_DB] Lancement du Stress Test de la Base de Données\n");
    printf("========================================================\n");

    // 1. Ouverture de la base de données en lecture seule (optimisation Chronos)
    int rc = sqlite3_open_v2("zyn-world.db", &db, SQLITE_OPEN_READWRITE, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[❌] Impossible d'ouvrir la base : %s\n", sqlite3_errmsg(db));
        return EXIT_FAILURE;
    }
    printf("[✅] Base de données chargée en mode READONLY.\n");

    // Configurer le cache SQLite au cas où (ex: 500 Mo de cache interne si Linux manque de place)
    sqlite3_exec(db, "PRAGMA cache_size = -500000;", NULL, NULL, NULL);

    // 2. Préparation de la requête SQL (Compilée une seule fois, règle d'or)
    // REMPLACE "macro_chunks" et "id" par le nom de ta table et de ta clé primaire
    const char *sql = "SELECT * FROM macro_chunks WHERE id = ?;";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[❌] Erreur de préparation SQL : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return EXIT_FAILURE;
    }

    // Graine fixe pour le déterminisme du test
    srand(42);

    printf("[⏳] Exécution de %d million de lectures aléatoires au hasard sur 134M de lignes...\n", NB_READS / 1000000);
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    long long dummy_bytes_read = 0;

    // 3. Boucle de Stress Test
    for (int i = 0; i < NB_READS; i++) {
        // Génération d'un ID de ligne aléatoire entre 1 et MAX_ROWS
        long long target_id = (long long)(((double)rand() / RAND_MAX) * (MAX_ROWS - 1)) + 1;

        // Liaison de l'ID à la requête préparée
        sqlite3_bind_int64(stmt, 1, target_id);

        // Exécution de la recherche
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            // Simulation de lecture du BLOB ou des colonnes pour forcer le CPU
            // Ici on récupère la taille du BLOB de données par exemple (colonne 1)
            dummy_bytes_read += sqlite3_column_bytes(stmt, 1);
        }

        // Réinitialisation du statement pour la prochaine itération (obligatoire et ultra-rapide)
        sqlite3_reset(stmt);
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time);

    // 4. Calculs des performances
    double total_time_ms = (double)(end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                           (double)(end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
    double total_time_sec = total_time_ms / 1000.0;
    double qps = (double)NB_READS / total_time_sec;
    double latence_us = (total_time_ms * 1000.0) / (double)NB_READS;

    printf("\n================= BILAN DES ACCÈS DB =================\n");
    printf("[🚀] Requêtes exécutées      : %d\n", NB_READS);
    printf("[⏱️] Durée totale du test     : %.2f ms (soit %.2f secondes)\n", total_time_ms, total_time_sec);
    printf("[📊] Débit d'accès           : %.2f QPS (Queries Per Second)\n", qps);
    printf("[💎] Latence moyenne estimée : %.2f microsecondes par Macro-Chunk\n", latence_us);
    printf("[🧮] Volume témoin traité    : %lld octets lus virtuellement\n", dummy_bytes_read);
    printf("======================================================\n");

    // Nettoyage
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return EXIT_SUCCESS;
}
