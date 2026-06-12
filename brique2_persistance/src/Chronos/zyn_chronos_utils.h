#ifndef ZYN_CHRONOS_UTILS_H
#define ZYN_CHRONOS_UTILS_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_chronos_utils.h
 * Date    : 12/06/2026
 * ============================================================================= */

#include <stdint.h>
#include <sqlite3.h>
#include <mqueue.h>
#include "zyn_b2_memory_pool.h"

#define RECV_BUFFER_SIZE 8

// Structure d'arguments unifiée pour isoler le contexte des sessions clients
typedef struct {
    int client_socket;
    SharedMemoryPoolHeader *pool;
    mqd_t recv_mq;
} __attribute__((packed)) ThreadArgs;

// Initialisations et configurations
void chronos_init_io_buffers(void);
void chronos_validate_arguments(int argc, char *argv[], char **shm_name);
SharedMemoryPoolHeader* chronos_map_shared_memory(const char *shm_name);
sqlite3* chronos_init_database(void);
int chronos_setup_server_socket(void);
static int32_t chronos_pop_shm_context(SharedMemoryPoolHeader *pool)

// Les routines de traitement (Turbines de Session)
void* chronos_reader_thread(void *arg);
void* chronos_writer_thread(void *arg);

#endif // ZYN_CHRONOS_UTILS_H
