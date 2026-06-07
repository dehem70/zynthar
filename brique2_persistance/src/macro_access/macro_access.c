/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 06/06/2026
 *
 * macro_access  :
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
#include "macro_access.h"
#include <stdio.h>

// Pointeur vers la requête préparée pour l'extraction flash
static sqlite3_stmt *stmt_extract_macro = NULL;

bool MacroAccess_Initialize(ZynDatabase *db) {
    if (!db || !db->is_connected || !db->handle) return false;

    // Requête SQL d'agrégation hautement optimisée avec jointure externe gauche (LEFT JOIN)
    // On extrait le BLOB de relief de la base principale et le BLOB de rivière de la base attachée
    const char *sql_query = 
        "SELECT c.data, r.data "
        "FROM macro_chunks c "
        "LEFT JOIN rivers.macro_chunks r ON c.id = r.id "
        "WHERE c.id = ?;";
    int rc = sqlite3_prepare_v3(db->handle, sql_query, -1, SQLITE_PREPARE_PERSISTENT, &stmt_extract_macro, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ZYN-MACRO] Échec de la préparation SQL : %s\n", sqlite3_errmsg(db->handle));
        stmt_extract_macro = NULL;
        return false;
    }

    printf("[ZYN-MACRO] Requête d'agrégation préparée avec succès (Mode PERSISTENT).\n");
    return true;
}

bool MacroAccess_GetPayload(ZynDatabase *db, uint32_t macro_id, MacroDataPayload *out_payload) {
    if (!db || !db->handle || !stmt_extract_macro || !out_payload) return false;

    // Réinitialisation de l'état de la requête préparée sans la recompiler
    sqlite3_reset(stmt_extract_macro);
    sqlite3_clear_bindings(stmt_extract_macro);

    // Liaison du macro_id (clé primaire INTEGER 32 bits) au point d'interrogation du SELECT
    if (sqlite3_bind_int64(stmt_extract_macro, 1, (sqlite3_int64)macro_id) != SQLITE_OK) {
        return false;
    }
    char id_str[32];
    int rc = sqlite3_step(stmt_extract_macro);
    if (rc == SQLITE_ROW) {
        // 1. Extraction du BLOB de relief (Table Core - Colonne 0)
        const void *core_blob = sqlite3_column_blob(stmt_extract_macro, 0);
        int core_bytes = sqlite3_column_bytes(stmt_extract_macro, 0);
        
        if (core_blob && core_bytes == 8) {
            // Lecture directe de la structure de 8 octets sans overhead en sautant les coordonnées locales
            // Offset 4 octets pour ignorer (region_x, region_z, macro_x, macro_z)
            const uint8_t *buffer = (const uint8_t *)core_blob;
            
            // Reconstitution rapide de notre structure métier
            // elevation_max_dm est stocké sur les octets 4 et 5 (int16_t)
            out_payload->elevation_max_dm = *(const int16_t *)(buffer + 4);
            out_payload->temperature_raw  = buffer[6];
            out_payload->biome            = buffer[7];
        } else {
            return false; // BLOB corrompu ou absent
        }

        // 2. Extraction du BLOB de rivière (Table Rivers - Colonne 1)
        if (sqlite3_column_type(stmt_extract_macro, 1) != SQLITE_NULL) {
            const void *river_blob = sqlite3_column_blob(stmt_extract_macro, 1);
            int river_bytes = sqlite3_column_bytes(stmt_extract_macro, 1);
            
            if (river_blob && river_bytes == 8) {
                const uint8_t *buffer = (const uint8_t *)river_blob;
                // data se trouve sur les 4 derniers octets de la structure ZynRiverNode
                out_payload->river_data = *(const uint32_t *)(buffer + 4);
                out_payload->has_river = true;
            } else {
                out_payload->river_data = 0;
                out_payload->has_river = false;
            }
        } else {
            // Le LEFT JOIN n'a rien trouvé : aucune rivière ne traverse ce Macro-Chunk
            out_payload->river_data = 0;
            out_payload->has_river = false;
        }
        return true; // Données extraites avec succès
    }

    // Aucun Macro-Chunk trouvé avec cet ID (Zone non générée ou en dehors du monde)
    return false;
}

void MacroAccess_Terminate(void) {
    if (stmt_extract_macro) {
        sqlite3_finalize(stmt_extract_macro);
        stmt_extract_macro = NULL;
        printf("[ZYN-MACRO] Requête d'agrégation libérée.\n");
    }
}
