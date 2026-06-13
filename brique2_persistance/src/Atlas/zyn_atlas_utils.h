#ifndef ZYN_ATLAS_UTILS_H
#define ZYN_ATLAS_UTILS_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_atlas_utils.h
 * Date    : 12/06/2026
 * ============================================================================= */

#include <stdint.h>
#include <mqueue.h>
#include "zyn_b2_memory_pool.h"


// Fonctions d'initialisation déportées du main
void atlas_init_io_buffers(void);
void atlas_validate_arguments(int argc, char *argv[], char **shm_name, int *event_fd);
SharedMemoryPoolHeader* atlas_map_shared_memory(const char *shm_name);
mqd_t atlas_open_chronos_queue(void);

// Fonctions d'orchestration Epoll et métier
int atlas_setup_epoll(int event_fd);
void atlas_compress_and_signal_page(SharedMemoryPoolHeader *pool, int32_t idx, mqd_t chronos_mq);

uint32_t atlas_compress_rle(const uint8_t *src, uint32_t src_size, uint8_t *dest, uint32_t max_dest_size);

#endif // ZYN_ATLAS_UTILS_H
