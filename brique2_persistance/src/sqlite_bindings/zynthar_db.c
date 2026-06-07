/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 06/06/2026
 *
 * zynthar_db  :
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
#include "zynthar_db.h"
#include <stdio.h>

// Définition des PRAGMA de performance critiques
static const char* zyn_pragmas[] = {
    "PRAGMA journal_mode = WAL;",            // Mode Write-Ahead Logging (Lectures non-bloquantes)
    "PRAGMA synchronous = NORMAL;",         // Sécurise le WAL sans bloquer sur l'I/O disque
    "PRAGMA temp_store = MEMORY;",          // Force les tables temporaires et tris en RAM
    "PRAGMA cache_size = -65536;",          // Alloue 64 MB de cache RAM par base (signe négatif = Ko)
    "PRAGMA mmap_size = 268435456;",        // Memory Map de 256 MB pour mapper l'index en espace virtuel OS
    "PRAGMA locking_mode = EXCLUSIVE;",     // Le serveur de jeu est le seul à toucher au fichier (gain CPU)
    NULL
};

// Fonction interne pour appliquer les PRAGMAs sur une base de données
static bool apply_performance_pragmas(sqlite3 *handle) {
    char *err_msg = NULL;
    for (int i = 0; zyn_pragmas[i] != NULL; i++) {
        if (sqlite3_exec(handle, zyn_pragmas[i], NULL, NULL, &err_msg) != SQLITE_OK) {
            fprintf(stderr, "[ZYN-DB] Erreur PRAGMA (%s) : %s\n", zyn_pragmas[i], err_msg);
            sqlite3_free(err_msg);
            return false;
        }
    }
    return true;
}

bool ZynDB_Initialize(ZynDatabase *db, const char *core_db_path, const char *rivers_db_path) {
    if (!db || !core_db_path || !rivers_db_path) return false;
    
    db->handle = NULL;
    db->is_connected = false;

    // 1. Ouverture de la base principale (world_core.db)
    // On utilise SQLITE_OPEN_NOMUTEX car le serveur gère la concurrence via sa file d'attente (Brique 1)
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX;
    if (sqlite3_open_v2(core_db_path, &(db->handle), flags, NULL) != SQLITE_OK) {
        fprintf(stderr, "[ZYN-DB] Impossible d'ouvrir la base Core : %s\n", sqlite3_errmsg(db->handle));
        ZynDB_Close(db);
        return false;
    }

    // 2. Application des PRAGMAs sur la base principale (main)
    if (!apply_performance_pragmas(db->handle)) {
        ZynDB_Close(db);
        return false;
    }

    // 3. Attachement de la base secondaire des rivières (world_rivers.db)
    // Utilisation d'un buffer statique sécurisé pour formater la requête ATTACH
    char attach_query[512];
    int n = snprintf(attach_query, sizeof(attach_query), "ATTACH DATABASE '%s' AS rivers;", rivers_db_path);
    if (n < 0 || n >= (int)sizeof(attach_query)) {
        fprintf(stderr, "[ZYN-DB] Chemin de base de données trop long.\n");
        ZynDB_Close(db);
        return false;
    }

    char *err_msg = NULL;
    if (sqlite3_exec(db->handle, attach_query, NULL, NULL, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "[ZYN-DB] Échec de l'ATTACH de la base Rivers : %s\n", err_msg);
        sqlite3_free(err_msg);
        ZynDB_Close(db);
        return false;
    }

    // 4. Application des PRAGMAs sur la base attachée
    // Par défaut, certains PRAGMA comme journal_mode ou synchronous doivent être réappliqués sur les schémas attachés
    if (sqlite3_exec(db->handle, "PRAGMA rivers.journal_mode = WAL;", NULL, NULL, &err_msg) != SQLITE_OK ||
        sqlite3_exec(db->handle, "PRAGMA rivers.synchronous = NORMAL;", NULL, NULL, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "[ZYN-DB] Erreur PRAGMA sur schéma Rivers : %s\n", err_msg);
        sqlite3_free(err_msg);
        ZynDB_Close(db);
        return false;
    }

    db->is_connected = true;
    printf("[ZYN-DB] Moteur de persistance initialisé avec succès. Mode Multi-Bases actif.\n");
    return true;
}

void ZynDB_Close(ZynDatabase *db) {
    if (!db || !db->handle) return;
    
    // Fermer le handle principal détache automatiquement les bases associées
    sqlite3_close_v2(db->handle);
    db->handle = NULL;
    db->is_connected = false;
    printf("[ZYN-DB] Connexions SQLite3 fermées proprement.\n");
}
