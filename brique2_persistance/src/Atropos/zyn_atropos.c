/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 09/06/2026
 *
 * zyn_atropos  :
 * utilisation :
 *
 * =============================================================================*/
  

#include <zynthar.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mqueue.h>
#include "zyn_b2_memory_pool.h"
#include "zyn_atropos_utils.h"
#include "zyn_atropos.h"

void atropos_run(SharedMemoryPoolHeader *pool, mqd_t mq) {
    printf("[📐 ATROPOS] Usine de découpe active et à l'écoute de la Message Queue.\n");
    AtroposMessage msg;
    unsigned int prio;

    while (1) {
        // 📥 LECTURE BLOQUANTE SÉCURISÉE
        if (mq_receive(mq, (char *)&msg, sizeof(AtroposMessage), &prio) == -1) {
            perror("[❌ ATROPOS] Erreur lors de la réception d'un message");
            usleep(1000);
            continue;
        }

        // 🎯 DISPATCHER DU POISON DE FERMETURE
        // Si l'index de page reçu est une sentinelle (ex: 255), on évacue proprement le processus.
        if (msg.shm_node_idx == 255) {
            printf("[📐 ATROPOS] Jeton de fermeture détecté. Extinction propre d'Atropos.\n");
            break;
        }

#if ZYN_LOG_DEBUG
        printf("[📐 ATROPOS] Signal reçu ! Découpe du Macro-Chunk en page SHM index : %d\n", msg.shm_node_idx);
#endif

        // 🔪 EXÉCUTION DE LA DÉCOUPE EN BUFFER FLUIDE
        atropos_slice_macro_chunk(&pool->atropos_queue, msg.shm_node_idx);
        
#if ZYN_LOG_DEBUG
        printf("[📐 ATROPOS] Découpe terminée. 4096 NanoJobs injectés pour la page %d.\n", msg.shm_node_idx);
#endif
    }
}

int main(int argc, char *argv[]) {
    char *shm_control_name = NULL;

    // 1. Initialisation des flux de sortie non-tamponnés
    atropos_init_io_buffers();
    printf("[📐 ATROPOS] Initialisation du processus indépendant...\n");

    // 2. Validation et extraction des paramètres d'entrée
    atropos_validate_arguments(argc, argv, &shm_control_name);

    // 3. Connexion et projection virtuelle de la SHM
    SharedMemoryPoolHeader *global_pool = atropos_map_shared_memory(shm_control_name);

    // 4. Ouverture de l'interface Message Queue IPC
    mqd_t mq = atropos_open_message_queue();

    printf("[✅ ATROPOS] Liaison SHM et Message Queue validées. Démarrage...\n");

    // 5. Entrée dans la boucle de traitement d'autorité
    atropos_run(global_pool, mq);

    // 6. Libération des ressources locales avant sortie
    mq_close(mq);

    return EXIT_SUCCESS;
}
