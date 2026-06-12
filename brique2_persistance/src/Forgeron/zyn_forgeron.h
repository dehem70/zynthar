#ifndef ZYN_FORGERON_H
#define ZYN_FORGERON_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_forgeron.h
 * Date    : 10/06/2026
 * ============================================================================= */

#include "zyn_b2_memory_pool.h"

/**
 * @brief Boucle principale du processus Forgeron.
 */
void forgeron_work(SharedMemoryPoolHeader *pool,int event_fd);

#endif // ZYN_FORGERON_H
