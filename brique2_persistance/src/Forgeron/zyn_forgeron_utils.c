/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 11/06/2026
 *
 * zyn_forgeron_utils  :
 * utilisation :
 *
 * =============================================================================*/
  
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h> 
#include <sys/eventfd.h>
#include <zynthar.h>
#include <errno.h>
#include <string.h>

#include "zyn_forgeron.h"


/* =============================================================================
 * SOUS-ROUTINES INTERNES DE LA TURBINE DE CALCUL (DÉPORTÉES DE FORGERON_WORK)
 * ============================================================================= */

/**
 * @brief Tente de réserver un ticket de lecture de manière atomique sur le Ring Buffer.
 * @return uint64_t Le numéro de ticket obtenu, ou la valeur sentinelle (uint64_t)-1 en cas d'échec (concurrence).
 */
uint64_t forgeron_try_reserve_ticket(AtroposRingBuffer *queue) {
    uint64_t current_head = __atomic_load_n(&queue->head, __ATOMIC_ACQUIRE);
    uint64_t current_tail = __atomic_load_n(&queue->tail, __ATOMIC_RELAXED);

    if (current_head >= current_tail) {
        return (uint64_t)-1; // Ring buffer vide
    }

    uint64_t expected_head = current_head;
    uint64_t desired_head = current_head + 1;

    if (!__atomic_compare_exchange_n(&queue->head, &expected_head, desired_head, 
                                     0, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED)) {
        return (uint64_t)-1; // Collision lock-free, un autre Forgeron a pris le ticket
    }

    return current_head;
}

/**
 * @brief Extrait un NanoJob du Ring Buffer en respectant le séquençage Vyukov
 * et libère immédiatement le slot pour les cycles futurs d'Atropos.
 */
NanoJob forgeron_extract_job(AtroposRingBuffer *queue, uint64_t ticket) {
    uint64_t slot = ticket & (NANO_QUEUE_SIZE - 1);

    // 🎯 SÉQUENÇAGE VYUKOV : On attend que le ticket_id soit signé par Atropos (ticket + 1)
    while (__atomic_load_n(&queue->buffer[slot].ticket_id, __ATOMIC_ACQUIRE) != (ticket + 1)) {
        __builtin_ia32_pause();
    }

    // Copie locale étanche du payload sur la pile du processus
    NanoJob job = queue->buffer[slot]; 

    // Relâchement du slot pour le prochain passage d'Atropos (dans un tour complet de queue)
    __atomic_store_n(&queue->buffer[slot].ticket_id, ticket + NANO_QUEUE_SIZE, __ATOMIC_RELEASE);

    return job;
}

/**
 * @brief Gère la comptabilité atomique de la page SHM et notifie Atlas si le lot est clos.
 */
void forgeron_finalize_job_lifecycle(SharedMemoryPoolHeader *pool, NanoJob job, int event_fd, uint64_t ticket, pid_t my_pid) {
    // Décrémentation atomique globale du nombre de jobs restants sur la page
    int32_t remaining = __atomic_sub_fetch(&pool->nodes[job.shm_node_idx].context.jobs_remaining, 1, __ATOMIC_SEQ_CST);

#if ZYN_LOG_DEBUG
    if (remaining == 4095) {
        printf("[⏱️ FORGERON %d] Ingestion initiale amorcée pour la Page SHM: %d (Ticket: %lu)\n", 
               my_pid, job.shm_node_idx, ticket);
    }
#endif

    // 🏁 FIN DE LA PAGE (Le compteur est tombé à 0, le bloc de 4096 voxels est fini)
    if (remaining <= 0) {
        // Alignement strict du type attendu pour le CAS sur le statut
        __typeof__(pool->nodes[job.shm_node_idx].context.status) expected_status = ZYN_STATUS_COMPUTING;

        // Validation atomique du basculement d'état vers READY
        if (__atomic_compare_exchange_n(&pool->nodes[job.shm_node_idx].context.status, 
                                         &expected_status, ZYN_STATUS_READY, 
                                         0, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED)) {
#if ZYN_LOG_DEBUG
            printf("[👑 FORGERON %d] Unité de production finalisée pour la page SHM %d.\n", 
                   my_pid, job.shm_node_idx);
#endif

            // Notification asynchrone transmise à Atlas (via l'EventFD capturé par epoll)
            uint64_t signal_val = 1;
            if (write(event_fd, &signal_val, sizeof(signal_val)) == -1) {
                perror("[❌ FORGERON] Échec de l'écriture sur EventFD");
            }
        }
    }
}

/**
 * @brief Configure la non-mise en mémoire tampon des flux d'I/O (Unbuffered)
 */
void forgeron_init_io_buffers(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
}

/**
 * @brief Valide les arguments passés par Cerbère au démarrage du processus
 */
void forgeron_validate_arguments(int argc, char *argv[], char **shm_name, int *event_fd) {
    if (argc < 3) {
        fprintf(stderr, "[❌ FORGERON] Erreur critique : Arguments d'initialisation manquants.\n");
        exit(EXIT_FAILURE);
    }
    *shm_name = argv[1];
    *event_fd = atoi(argv[2]);
}

/**
 * @brief Gère la connexion et le mapping virtuel de la mémoire partagée maîtresse
 */
SharedMemoryPoolHeader* forgeron_map_shared_memory(const char *shm_name) {
    int ctrl_fd = shm_open(shm_name, O_RDWR, 0666);
    if (ctrl_fd == -1) {
        perror("[❌ FORGERON] Erreur de connexion au segment de contrôle SHM");
        exit(EXIT_FAILURE);
    }

    SharedMemoryPoolHeader *pool = (SharedMemoryPoolHeader*)mmap(
        NULL, sizeof(SharedMemoryPoolHeader), PROT_READ | PROT_WRITE, MAP_SHARED, ctrl_fd, 0
    );

    // Libération immédiate du descripteur de fichier Unix après mapping réussi
    close(ctrl_fd);

    if (pool == MAP_FAILED) {
        perror("[❌ FORGERON] Échec critique du mapping (mmap) de la mémoire partagée");
        exit(EXIT_FAILURE);
    }

    return pool;
}

