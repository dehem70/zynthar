#ifndef ZYN_CHRONOS_H
#define ZYN_CHRONOS_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_chronos.h
 * Date    : 07/06/2026
 * ============================================================================= */
 
 #include <mqueue.h>
#include <sqlite3.h>
#include <netinet/in.h>
#include "zyn_b2_memory_pool.h" // 🎯 AJOUT pour que Chronos connaisse la structure SHM

/**
 * @brief Point d'entrée de la boucle réseau et d'accès SQLite3 de Chronos.
 * @param pool Pointeur mappé vers le segment de contrôle SHM maître.
 */
void chronos_run(SharedMemoryPoolHeader *pool, int server_fd, struct sockaddr_in address, int addrlen, mqd_t atropos_mq);

static uint8_t* chronos_get_and_map_page(SharedMemoryPoolHeader *pool, int32_t idx);
static void chronos_push_shm_context(SharedMemoryPoolHeader *pool, int32_t free_idx);
static int32_t chronos_pop_shm_context(SharedMemoryPoolHeader *pool);


/* * Structure du paquet de demande (Request Packet)
 * Alignement strict, calqué sur le référentiel et le simulateur B3.
 */
typedef struct __attribute__((packed)) {
    uint32_t macro_chunk_id;
    uint8_t mc_x;
    uint8_t mc_y;
    uint8_t mc_z;
    uint8_t lod;
} ChunkRequestPacket;

typedef struct {
    SharedMemoryPoolHeader *pool;
    int socket_fd;
    mqd_t atropos_mq;
    mqd_t recv_mq;
    sqlite3_stmt *stmt;
} ChronosSessionArgs;


#endif // ZYN_CHRONOS_H
