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
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mqueue.h>
#include <zynthar.h>

#include "zyn_atropos.h"

// On déporte la fonction de découpe qu'on a validée ensemble
void atropos_slice_macro_chunk(AtroposRingBuffer *queue, uint8_t shm_idx) {
    uint64_t current_tail = queue->tail;

    for (uint8_t y = 0; y < 16; y++) {
        for (uint8_t z = 0; z < 16; z++) {
            for (uint8_t x = 0; x < 16; x++) {
                
                // Sécurité Backpressure sur le tapis des Forgerons
                while ((current_tail - __atomic_load_n(&queue->head, __ATOMIC_ACQUIRE)) >= NANO_QUEUE_SIZE) {
                    usleep(10); 
                }

                uint64_t slot = current_tail & (NANO_QUEUE_SIZE - 1);

                queue->buffer[slot].shm_node_idx = shm_idx;
                queue->buffer[slot].sub_x = x;
                queue->buffer[slot].sub_y = y;
                queue->buffer[slot].sub_z = z;

                current_tail++;
            }
        }
    }

    // Poussée atomique : les Forgerons voient les 4096 jobs
    __atomic_store_n(&queue->tail, current_tail, __ATOMIC_RELEASE);
}

void atropos_run(SharedMemoryPoolHeader *pool, mqd_t mq) {
    printf("[📐 ATROPOS] Usine de découpe active et à l'écoute de la Message Queue.\n");
    AtroposMessage msg;
    unsigned int prio;
    while (1) {
        // 📥 LECTURE BLOQUANTE
        // Le processus s'endort ici à 0% CPU tant que Chronos n'envoie rien.
        // Dès qu'un message arrive, Linux le réveille instantanément.
        if (mq_receive(mq, (char *)&msg, sizeof(AtroposMessage), &prio) == -1) {
            perror("[❌ ATROPOS] Erreur lors de la réception d'un message");
            usleep(1000); // Anti-boucle folle en cas d'erreur système
            continue;
        }

        printf("[📐 ATROPOS] Signal reçu ! Découpe du Macro-Chunk en page SHM index : %d\n", msg.shm_node_idx);

        // 🔪 EXÉCUTION DE LA DÉCOUPE
        // On passe à la fonction le Ring Buffer (qui sera dans le pool SHM) et l'index reçu
        atropos_slice_macro_chunk(&pool->atropos_queue, msg.shm_node_idx);
        
        printf("[📐 ATROPOS] Découpe terminée. 4096 NanoJobs injectés pour la page %d.\n", msg.shm_node_idx);
    }
}

int main(int argc, char *argv[]) {
    printf("[📐 ATROPOS] Initialisation du processus indépendant...\n");

    if (argc < 2) {
        fprintf(stderr, "[❌ ATROPOS] Erreur : Nom du segment de contrôle SHM manquant.\n");
        return EXIT_FAILURE;
    }

    char *shm_control_name = argv[1];

    // Connexion au segment de contrôle maître créé par Cerbère
    int ctrl_fd = shm_open(shm_control_name, O_RDWR, 0666);
    if (ctrl_fd == -1) {
        perror("[❌ ATROPOS] Erreur de connexion au segment SHM");
        return EXIT_FAILURE;
    }

    SharedMemoryPoolHeader *global_pool = (SharedMemoryPoolHeader*)mmap(
        NULL, sizeof(SharedMemoryPoolHeader), PROT_READ | PROT_WRITE, MAP_SHARED, ctrl_fd, 0
    );
    close(ctrl_fd);

    if (global_pool == MAP_FAILED) {
        perror("[❌ ATROPOS] Échec du mmap global");
        return EXIT_FAILURE;
    }

    // 2. Connexion à la Message Queue en Lecture Seule (O_RDONLY)
    mqd_t mq = mq_open(ZYN_ATROPOS_MQ_NAME, O_RDONLY);
    if (mq == (mqd_t)-1) {
        perror("[❌ ATROPOS] Erreur critique : Impossible d'ouvrir la Message Queue");
        return EXIT_FAILURE;
    }

    printf("[✅ ATROPOS] Liaison SHM et Message Queue validées. Démarrage...\n");

    // Lancement de la logique métier
    atropos_run(global_pool,mq);

    return EXIT_SUCCESS;
}
