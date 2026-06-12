/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 12/06/2026
 *
 * zyn_atropos_utils  :
 * utilisation :
 *
 * =============================================================================*/
  
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <zynthar.h>
#include "zyn_atropos_utils.h"

void atropos_init_io_buffers(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
}

void atropos_validate_arguments(int argc, char *argv[], char **shm_name) {
    if (argc < 2) {
        fprintf(stderr, "[❌ ATROPOS] Erreur critique : Nom du segment de contrôle SHM manquant.\n");
        exit(EXIT_FAILURE);
    }
    *shm_name = argv[1];
}

SharedMemoryPoolHeader* atropos_map_shared_memory(const char *shm_name) {
    int ctrl_fd = shm_open(shm_name, O_RDWR, 0666);
    if (ctrl_fd == -1) {
        perror("[❌ ATROPOS] Erreur de connexion au segment SHM");
        exit(EXIT_FAILURE);
    }

    SharedMemoryPoolHeader *pool = (SharedMemoryPoolHeader*)mmap(
        NULL, sizeof(SharedMemoryPoolHeader), PROT_READ | PROT_WRITE, MAP_SHARED, ctrl_fd, 0
    );
    close(ctrl_fd);

    if (pool == MAP_FAILED) {
        perror("[❌ ATROPOS] Échec critique du mmap global");
        exit(EXIT_FAILURE);
    }
    return pool;
}

mqd_t atropos_open_message_queue(void) {
    // Ouverte en O_RDWR pour permettre l'envoi éventuel d'un jeton poison d'extinction
    mqd_t mq = mq_open(ZYN_ATROPOS_MQ_NAME, O_RDWR);
    if (mq == (mqd_t)-1) {
        perror("[❌ ATROPOS] Erreur critique : Impossible d'ouvrir la Message Queue");
        exit(EXIT_FAILURE);
    }
    return mq;
}

void atropos_slice_macro_chunk(AtroposRingBuffer *queue, uint8_t shm_idx) {
    // 1. Chargement atomique sécurisé avec Cast pour Clang/IWYU
    uint64_t current_tail = __atomic_load_n((uint64_t *)&queue->tail, __ATOMIC_ACQUIRE);

    // 🛑 BARRIÈRE D'ENTRÉE GLOBAL : Attente de place
    while ((current_tail + 4096 - __atomic_load_n((uint64_t *)&queue->head, __ATOMIC_ACQUIRE)) > NANO_QUEUE_SIZE) {
        usleep(50);
    }

    // 2. ÉCRITURE EN RAFALE SÉQUENCÉE (Moteur Vyukov)
    for (uint8_t y = 0; y < 16; y++) {
        for (uint8_t z = 0; z < 16; z++) {
            for (uint8_t x = 0; x < 16; x++) {
                
                uint64_t slot = current_tail & (NANO_QUEUE_SIZE - 1);

                // 🎯 VERROU DE SLOT ÉTANCHE (Anti-Lap Overwrite) + Cast IWYU
                while (__atomic_load_n((uint64_t *)&queue->buffer[slot].ticket_id, __ATOMIC_ACQUIRE) != current_tail) {
                    __builtin_ia32_pause();
                }

                // Écriture du payload
                queue->buffer[slot].shm_node_idx = shm_idx;
                queue->buffer[slot].sub_x = x;
                queue->buffer[slot].sub_y = y;
                queue->buffer[slot].sub_z = z;

                // 🏁 SIGNATURE ET PASSE DE TÉMOIN + Cast IWYU
                __atomic_store_n((uint64_t *)&queue->buffer[slot].ticket_id, current_tail + 1, __ATOMIC_RELEASE);

                current_tail++;
            }
        }
    }

    // 3. 🚀 FLUSH UNIQUE GLOBAL + Cast IWYU
    __atomic_store_n((uint64_t *)&queue->tail, current_tail, __ATOMIC_RELEASE);
}
