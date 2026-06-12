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
#include "zyn_chronos.h"

#define RECV_BUFFER_SIZE 8
void handle_shutdown_signal(int sig);

// Fonction spécialisée dans la requête SQLite et l'injection des données dans la page SHM
void chronos_fetch_and_populate_context(sqlite3_stmt *stmt, SharedMemoryPoolHeader *pool, int32_t idx, int64_t macro_chunk_id);
void* chronos_reader_thread(void *arg);
// Fonction isolée gérant la cinématique de notification résiliente vers Atropos
void chronos_notify_atropos(mqd_t atropos_mq, int32_t idx);

// Fonction spécialisée dans l'expédition Non-Bloquante avec gestion de la Backpressure TCP
int chronos_send_payload_nonblocking(int socket_fd, const uint8_t *voxels_page, uint32_t size, uint64_t macro_id);
void* chronos_writer_thread(void *arg);
/**
 * @brief Pioche un nœud libre dans le pool partagé de manière bloquante / adaptative.
 * @return L'index du nœud pioché (0 à MAX_POOL_PAGES-1).
 */
int32_t chronos_pop_shm_context(SharedMemoryPoolHeader *pool);
void chronos_push_shm_context(SharedMemoryPoolHeader *pool, int32_t free_idx);
/**
 * @brief Récupère ou associe l'adresse virtuelle locale de la page de 16 Mo spécifiée par l'index.
 */
uint8_t* chronos_get_and_map_page(SharedMemoryPoolHeader *pool, int32_t idx);
// A. Vérification et création du dossier de stockage physique
void chronos_ensure_storage_tree(void);
// A. Centralisation exclusive de l'assemblage des chemins d'accès Ramdisk
void chronos_prepare_db_paths(char *path_world, char *path_river, char *path_delta, size_t max_len);

// A. Boucle dynamique d'ouverture et de raccordement (ATTACH) des bases
void chronos_setup_sqlite_engine(const ChronosDatabaseTarget *targets, size_t target_count);
// B. Injection isolée des PRAGMAs de compétition pour la performance pure
void chronos_inject_performance_pragmas(void);
// C. Compilation de la requête d'autorité et raccordement au tunnel Message Queue
sqlite3_stmt* chronos_bind_ipc_tunnels(mqd_t *recv_mq);


sqlite3_stmt* chronos_init_storage_and_tunnels(mqd_t *recv_mq);
 
 // A. Validation des arguments de la ligne de commande
void chronos_parse_args(int argc, char *argv[], char **shm_name);

// B. Projection du segment de mémoire partagée maître
SharedMemoryPoolHeader* chronos_attach_shm(const char *shm_name);

// C. Création et configuration de la Socket Serveur TCP
int chronos_create_server_socket(struct sockaddr_in *address, int *addrlen);

void chronos_run(SharedMemoryPoolHeader *pool, int server_fd, struct sockaddr_in address, int addrlen, mqd_t atropos_mq);

#endif
