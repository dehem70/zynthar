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
