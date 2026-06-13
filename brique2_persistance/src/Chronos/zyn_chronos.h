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
#include "zyn_b2_memory_pool.h"



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
    const char *path;       // Chemin physique sur le Ramdisk (ex: path_river)
    const char *alias;      // Nom logique dans SQLite (ex: "rivers")
    uint8_t is_main;        // 1 pour la base maîtresse principale, 0 pour un ATTACH
} ChronosDatabaseTarget;

typedef struct {
    SharedMemoryPoolHeader *pool;
    int socket_fd;
    mqd_t atropos_mq;
    mqd_t recv_mq;
    sqlite3_stmt *stmt;
} ChronosSessionArgs;


#endif // ZYN_CHRONOS_H
