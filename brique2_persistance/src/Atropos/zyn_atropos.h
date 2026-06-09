#ifndef ZYN_ATROPOS_H
#define ZYN_ATROPOS_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_atropos.h
 * Date    : 09/06/2026
 * ============================================================================= */

#include <stdint.h>
#include "zyn_b2_memory_pool.h"

/**
 * @brief Point d'entrée de la boucle de traitement d'Atropos.
 * @param pool Pointeur vers la mémoire partagée maîtresse.
 */
void atropos_run(SharedMemoryPoolHeader *pool, mqd_t mq);
#endif // ZYN_ATROPOS_H
