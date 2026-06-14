#ifndef ZYN_CERBERE_H
#define ZYN_CERBERE_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_cerbere.h
 * Date    : 07/06/2026
 * ============================================================================= */
 
 #include <stdint.h>
#include "zyn_b2_memory_pool.h"
 
/**
 * @brief Configuration de staging extraite du fichier de configuration global.
 */
typedef struct {
    char root_path[512];
    uint32_t backup_interval_seconds;
} CerbereConfig;

/**
 * @brief Arguments d'encapsulation de contexte pour le Worker asynchrone de Cerbère.
 * Aligné pour limiter la contamination des lignes de cache.
 */
typedef struct {
    char src_deltas[512];
    char dst_deltas[512];
    uint32_t interval_sec;
    volatile int keep_running;
} WatchdogArgs;

 
/**
 * @brief Initialise le Ramdisk, charge les bases de données SQLite3 en RAM
 * et démarre le thread de flush asynchrone.
 * @return 0 en cas de succès, -1 si une erreur critique survient.
 */
int cerbere_init(void);

/**
 * @brief Force la synchronisation finale du Delta sur le disque persistant
 * et termine proprement le processus.
 */
void cerbere_shutdown(void);

static int cerbere_allocate_shm_page(SharedMemoryPoolHeader *pool, int32_t idx);

void cerbere_init_shm_pool(SharedMemoryPoolHeader *pool, int32_t initial_size);
void* cerbere_shm_monitoring_thread(void *arg);
void cerbere_cleanup_all_shm(SharedMemoryPoolHeader *pool) ;
#endif // ZYN_CERBERE_H
