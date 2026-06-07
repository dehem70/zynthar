#ifndef ZYN_MACRO_CHUNK_INJECTOR_H
#define ZYN_MACRO_CHUNK_INJECTOR_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_macro_chunk_injector.h
 * Date    : 06/06/2026
 * ============================================================================= */
#include <stdint.h>
#include <stddef.h>
#include <zynthar.h>
#include "zyn_river_agent.h"

typedef union {
    struct {
        uint8_t z;    // Octet de poids faible (Bits 0-7)
        uint8_t x;    // (Bits 8-15)
        uint8_t rz;   // (Bits 16-23)
        uint8_t rx;   // Octet de poids fort (Bits 24-31)
    };
    uint32_t id;     // Les 4 octets combinés en un seul entier
} Id;

/**
 * @brief Injecte un tableau de macro-chunks dans la base de données.
 * Écrase silencieusement les données existantes en cas de conflit de clé primaire.
 * Utilise une transaction unique pour maximiser le débit d'écriture (Bulk Insert).
 * * @param chunks Pointeur vers le tableau de MacroChunk.
 * @param count Nombre d'éléments dans le tableau.
 * @return int 0 en cas de succès, un code d'erreur non-nul sinon.
 */
int zyn_inject_macro_chunks(const MacroChunk* chunks, size_t count);
int zyn_inject_macro_river(const ZynRiverNode*   flux_grid, size_t count);
int zyn_store_world_metadata(uint32_t seed);

#endif // ZYN_MACRO_CHUNK_INJECTOR_H
