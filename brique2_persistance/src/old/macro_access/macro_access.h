#ifndef MACRO_ACCESS_H
#define MACRO_ACCESS_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : macro_access.h
 * Date    : 06/06/2026
 * ============================================================================= */

#include <stdint.h>
#include <stdbool.h>
#include "zynthar_db.h"

// Structure de destination qui agrège les données brutes des deux BDD
typedef struct {
    uint8_t  biome;
    uint8_t  temperature_raw;
    int16_t  elevation_max_dm;
    uint32_t river_data;       // Flow + Direction packés sur 4 octets
    bool     has_river;        // Flag pour savoir si le noeud hydrographique existe
} MacroDataPayload;

// Initialisation et destruction du module (préparation des requêtes SQL)
bool MacroAccess_Initialize(ZynDatabase *db);
void MacroAccess_Terminate(void);

// Fonction de lecture flash (Zéro allocation RAM en cours de jeu)
bool MacroAccess_GetPayload(ZynDatabase *db, uint32_t macro_id, MacroDataPayload *out_payload);

#endif // MACRO_ACCESS_H
