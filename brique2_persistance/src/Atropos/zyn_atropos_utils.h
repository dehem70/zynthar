#ifndef ZYN_ATROPOS_UTILS_H
#define ZYN_ATROPOS_UTILS_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_atropos_utils.h
 * Date    : 12/06/2026
 * ============================================================================= */

#include <stdint.h>
#include <mqueue.h>
#include "zyn_b2_memory_pool.h"
#include "zyn_atropos.h"


// Fonctions d'initialisation déportées du main
void atropos_init_io_buffers(void);
void atropos_validate_arguments(int argc, char *argv[], char **shm_name);
SharedMemoryPoolHeader* atropos_map_shared_memory(const char *shm_name);
mqd_t atropos_open_message_queue(void);

// Fonctions cœur de découpe
void atropos_slice_macro_chunk(AtroposRingBuffer *queue, uint8_t shm_idx);

#endif // ZYN_ATROPOS_UTILS_H

