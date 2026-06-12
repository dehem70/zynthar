/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 10/06/2026
 *
 * zyn_forgeron  :
 * utilisation :
 *
 * =============================================================================*/
  
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> 
#include <unistd.h>

#include <zynthar.h>
#include "zyn_forgeron.h"
#include "zyn_forgeron_utils.h"

/* =============================================================================
 * ROUTINE MAÎTRESSE
 * ============================================================================= */
void forgeron_work(SharedMemoryPoolHeader *pool, int event_fd) {
    AtroposRingBuffer *queue = &pool->atropos_queue;
    pid_t my_pid = getpid();
    
    printf("[🛠️ FORGERON %d] Déploiement opérationnel dans le pool de calcul.\n", my_pid);

    while (1) {
        // 1. Essai de réservation d'un ticket sur le tapis roulant
        uint64_t ticket = forgeron_try_reserve_ticket(queue);
        if (ticket == (uint64_t)-1) {
            // Le tapis est vide ou un autre processus a été plus rapide : micro-respiration
            usleep(250); 
            continue;
        }

        // 2. Extraction sécurisée du job via la barrière Vyukov
        NanoJob job = forgeron_extract_job(queue, ticket);

        // 🎯 C'EST ICI que se branchera la future fonction de calcul/génération procédurale 
        // ex: forgeron_execute_procedural_generation(job);

        // 3. Traitement comptable de la page et notification d'autorité (EventFD)
        forgeron_finalize_job_lifecycle(pool, job, event_fd, ticket, my_pid);
    }
}

/* =============================================================================
 * POINT D'ENTRÉE PRINCIPAL (ORCHESTRATEUR ÉPURÉ)
 * ============================================================================= */
int main(int argc, char *argv[]) {
    char *shm_control_name = NULL;
    int event_fd = -1;

    // 1. Configuration des flux de sortie standards
    forgeron_init_io_buffers();

    // 2. Extraction et validation des paramètres d'injection
    forgeron_validate_arguments(argc, argv, &shm_control_name, &event_fd);

    // 3. Connexion et projection de la forteresse Lock-Free en RAM
    SharedMemoryPoolHeader *global_pool = forgeron_map_shared_memory(shm_control_name);

    // 4. Lancement de la turbine de calcul (routine de cueillette)
    forgeron_work(global_pool, event_fd);

    return EXIT_SUCCESS;
}
