#ifndef ZYNTHAR_DB_H
#define ZYNTHAR_DB_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zynthar_db.h
 * Date    : 06/06/2026
 * ============================================================================= */
#include <stdint.h>
#include <stdbool.h>
#include <sqlite3.h>

// Handle principal de la connexion de persistance
typedef struct {
    sqlite3 *handle;
    bool is_connected;
} ZynDatabase;

// Fonctions d'interface de la Brique 2
bool ZynDB_Initialize(ZynDatabase *db, const char *core_db_path, const char *rivers_db_path);
void ZynDB_Close(ZynDatabase *db);

#endif // ZYNTHAR_DB_H
