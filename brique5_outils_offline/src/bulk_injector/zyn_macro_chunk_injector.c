/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 06/06/2026
 *
 * zyn_macro_chunk_injector  :
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
#include "zyn_macro_chunk_injector.h"

#define PATH_MAX_BUFFER 1024
#include <time.h>




int zyn_inject_macro_chunks(const MacroChunk* chunks, size_t count) {
    clock_t start_global, end_global;
    start_global = clock();
    if (!chunks || count == 0) {
        return -1;
    }

    // 1. Récupération des variables d'environnement pour le chemin de la DB
    const char* zyn_root = getenv("ZYNTHAR_ROOT");
    if (!zyn_root) {
        fprintf(stderr, "[ERROR] L'environnement ZYNTHAR_ROOT n'est pas défini.\n");
        return -2;
    }

    char db_path[PATH_MAX_BUFFER];
    // Construction du chemin via ZYN_DB_EMPLACEMENT / ZYN_DB_WORLD
    int written = snprintf(db_path, sizeof(db_path), "%s/%s%s", zyn_root,ZYN_DB_EMPLACEMENT,ZYN_DB_WORLD);
    if (written >= (int)sizeof(db_path)) {
        fprintf(stderr, "[ERROR] Le chemin de la base de données est trop long.\n");
        return -3;
    }

    // 2. Ouverture de la base de données (Doit déjà exister)
    sqlite3* db = NULL;
    int rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] Impossible d'ouvrir la DB à : %s (%s)\n", db_path, sqlite3_errmsg(db));
        if (db) sqlite3_close(db);
        return -4;
    }

    // 3. Configuration des PRAGMA de performance (WAL et synchronous = NORMAL)
    // =============================================================================
    // 3. CONFIGURATION DES PRAGMA ULTRA-PERFORMANCE (BOOST OFFLINE)
    // =============================================================================
    // WAL permet des écritures concurrentes et plus fluides
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    
    // OFF au lieu de NORMAL : IDÉAL POUR LE PIPELINE OFFLINE. 
    // On ne synchronise plus le fichier à chaque étape, le gain de temps est massif.
    sqlite3_exec(db, "PRAGMA synchronous=OFF;", NULL, NULL, NULL);
    
    // Augmentation drastique du cache mémoire de SQLite. 
    // -524288 spécifie ~512 Mo de mémoire cache (valeur négative = en Kio).
    sqlite3_exec(db, "PRAGMA cache_size=-524288;", NULL, NULL, NULL);
    
    // Désactive le verrouillage système inutile pour un outil mono-thread offline
    sqlite3_exec(db, "PRAGMA locking_mode=EXCLUSIVE;", NULL, NULL, NULL);
    // =============================================================================

    // 4. Préparation de la requête avec INSERT OR REPLACE pour l'écrasement silencieux
    const char* query = "INSERT OR REPLACE INTO macro_chunks "
                        "(id, data) "
                        "VALUES (?, ?);";
    sqlite3_stmt* stmt = NULL;
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] Échec de préparation du statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -5;
    }

    // 5. Démarrage de la transaction massive
    rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] Impossible de démarrer la transaction: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -6;
    }

    // 6. Boucle d'injection rapide
    for (size_t i = 0; i < count; i++) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        Id loc;
        loc.rx=chunks[i].region_x;
        loc.rz=chunks[i].region_z;
        loc.x=chunks[i].chunk_x;
        loc.z=chunks[i].chunk_z;
        uint32_t macro_id = loc.id;
        sqlite3_bind_int(stmt, 1, macro_id);
        sqlite3_bind_blob(stmt, 2, (const void *) &chunks[i], sizeof(MacroChunk), SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "[ERROR] Erreur lors du Bulk Insert à l'index %zu: %s\n", i, sqlite3_errmsg(db));
            // Rollback en cas de pépin pour éviter de corrompre l'état de la v0.1
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return -7;
        }
    }

    // 7. Validation de la transaction
    rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] Échec du COMMIT de la transaction: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -8;
    }

    // Nettoyage propre sans fuite de mémoire
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    end_global = clock();
    double temps_total = ((double)(end_global - start_global)) / CLOCKS_PER_SEC;
    PRINT_BOTH("\n=====================================================================\n");
    PRINT_BOTH(" INJECTION DANS LA BASE RELIEF EN %.4f SECONDES\n", temps_total);
    PRINT_BOTH("=====================================================================\n");
    return 0;
}


int zyn_inject_macro_river(const ZynRiverNode*   flux_grid, size_t count) {
    clock_t start_global, end_global;
    start_global = clock();
    if (!flux_grid || count == 0) {
        return -1;
    }

    // 1. Récupération des variables d'environnement pour le chemin de la DB
    const char* zyn_root = getenv("ZYNTHAR_ROOT");
    if (!zyn_root) {
        fprintf(stderr, "[ERROR] L'environnement ZYNTHAR_ROOT n'est pas défini.\n");
        return -2;
    }

    char db_path[PATH_MAX_BUFFER];
    // Construction du chemin via ZYN_DB_EMPLACEMENT / ZYN_DB_WORLD
    int written = snprintf(db_path, sizeof(db_path), "%s/%s%s", zyn_root,ZYN_DB_EMPLACEMENT,ZYN_DB_RIVER);
    if (written >= (int)sizeof(db_path)) {
        fprintf(stderr, "[ERROR] Le chemin de la base de données est trop long.\n");
        return -3;
    }

    // 2. Ouverture de la base de données (Doit déjà exister)
    sqlite3* db = NULL;
    int rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] Impossible d'ouvrir la DB à : %s (%s)\n", db_path, sqlite3_errmsg(db));
        if (db) sqlite3_close(db);
        return -4;
    }

    // 3. Configuration des PRAGMA de performance (WAL et synchronous = NORMAL)
    // =============================================================================
    // 3. CONFIGURATION DES PRAGMA ULTRA-PERFORMANCE (BOOST OFFLINE)
    // =============================================================================
    // WAL permet des écritures concurrentes et plus fluides
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    
    // OFF au lieu de NORMAL : IDÉAL POUR LE PIPELINE OFFLINE. 
    // On ne synchronise plus le fichier à chaque étape, le gain de temps est massif.
    sqlite3_exec(db, "PRAGMA synchronous=OFF;", NULL, NULL, NULL);
    
    // Augmentation drastique du cache mémoire de SQLite. 
    // -524288 spécifie ~512 Mo de mémoire cache (valeur négative = en Kio).
    sqlite3_exec(db, "PRAGMA cache_size=-524288;", NULL, NULL, NULL);
    
    // Désactive le verrouillage système inutile pour un outil mono-thread offline
    sqlite3_exec(db, "PRAGMA locking_mode=EXCLUSIVE;", NULL, NULL, NULL);
    // =============================================================================

    // 4. Préparation de la requête avec INSERT OR REPLACE pour l'écrasement silencieux
    const char* query = "INSERT OR REPLACE INTO macro_chunks "
                        "(id, data) "
                        "VALUES (?, ?);";
    sqlite3_stmt* stmt = NULL;
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] Échec de préparation du statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -5;
    }

    // 5. Démarrage de la transaction massive
    rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] Impossible de démarrer la transaction: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -6;
    }

    // 6. Boucle d'injection rapide
    for (size_t i = 0; i < count; i++) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        Id loc;
        loc.rx=flux_grid[i].region_x;
        loc.rz=flux_grid[i].region_z;
        loc.x=flux_grid[i].macro_x;
        loc.z=flux_grid[i].macro_z;
        uint32_t macro_id = loc.id;
        sqlite3_bind_int(stmt, 1, macro_id);
        sqlite3_bind_blob(stmt, 2, (const void *) &flux_grid[i], sizeof(ZynRiverNode), SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "[ERROR] Erreur lors du Bulk Insert à l'index %zu: %s\n", i, sqlite3_errmsg(db));
            // Rollback en cas de pépin pour éviter de corrompre l'état de la v0.1
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return -7;
        }
    }

    // 7. Validation de la transaction
    rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] Échec du COMMIT de la transaction: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -8;
    }

    // Nettoyage propre sans fuite de mémoire
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    end_global = clock();
    double temps_total = ((double)(end_global - start_global)) / CLOCKS_PER_SEC;
    PRINT_BOTH("\n=====================================================================\n");
    PRINT_BOTH(" INJECTION DANS LA BASE RIVIERE EN %.4f SECONDES\n", temps_total);
    PRINT_BOTH("=====================================================================\n");
    return 0;
}

