/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 10/06/2026
 *
 * zyn_atlas  :
 * utilisation :
 *
 * =============================================================================*/
  
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <mqueue.h>
#include <errno.h>
#include <signal.h>

#include <zynthar.h>
#include "zyn_b2_memory_pool.h"
#include "zyn_atlas_utils.h"

#define MAX_EVENTS 1

// Drapeau d'arrêt volatile pour la respiration d'Epoll
static volatile sig_atomic_t g_atlas_shutdown = 0;

// Gestionnaire de capture pour le signal d'arrêt propagé par le parent
void atlas_handle_signal(int sig) {
    (void)sig;
    g_atlas_shutdown = 1;
}

void atlas_run(SharedMemoryPoolHeader *pool, int event_fd) {
    printf("[🌍 ATLAS] Moteur réseau et compression actif (Écoute Epoll configurée).\n");
    
    mqd_t chronos_mq = atlas_open_chronos_queue();
    int epoll_fd = atlas_setup_epoll(event_fd);

    struct epoll_event events[MAX_EVENTS];
    uint64_t signal_counter = 0;
    int32_t last_scanned_idx = 0; 

    // 🎯 PROTECTION ET AUTONOMIE : Atlas s'arrête si le signal est levé OU si Cerbère (parent) est mort (PID == 1)
    while (!g_atlas_shutdown) { 
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 100);
        
        if (nfds == -1) {
            if (errno == EINTR) {
                break; 
            }
            perror("[❌ ATLAS] Erreur lors de l'epoll_wait");
            continue;
        }

        // Si timeout (100 ms passées sans activité), on remonte au while
        // qui va immédiatement vérifier si getppid() est tombé à 1 !
        if (nfds == 0) {
            continue; 
        }

        if (read(event_fd, &signal_counter, sizeof(signal_counter)) == -1) {
            perror("[❌ ATLAS] Erreur de lecture sur l'eventfd");
            continue;
        }

        while (signal_counter > 0 && !g_atlas_shutdown) {
            int32_t pages_processed_this_turn = 0;

            for (int i = 0; i < MAX_POOL_PAGES; i++) {
                int idx = (last_scanned_idx + i) % MAX_POOL_PAGES;
                
                uint8_t current_status = __atomic_load_n((uint8_t *)&pool->nodes[idx].context.status, __ATOMIC_ACQUIRE);

                if (current_status == ZYN_STATUS_READY) {
                    last_scanned_idx = (idx + 1) % MAX_POOL_PAGES;
                    pages_processed_this_turn = 1;
                    signal_counter--; 

                    atlas_compress_and_signal_page(pool, idx, chronos_mq);
                    break; 
                }
            }

            if (pages_processed_this_turn == 0) {
                signal_counter = 0; 
            }
        }
    }

    // 🧹 SORTIE PROPRE ET SÉCURISÉE
    printf("[🌍 ATLAS] Ordre d'extinction détecté. Nettoyage et fermeture d'Atlas.\n");
    close(epoll_fd);
    mq_close(chronos_mq);
}


int main(int argc, char *argv[]) {
    char *shm_control_name = NULL;
    int event_fd = -1;
    
    // 🎯 CONFIGURATION DU CAPTEUR DE SIGNAUX (Fermeture douce)
    struct sigaction sa;
    sa.sa_handler = atlas_handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);  // Capture du Ctrl+C
    sigaction(SIGTERM, &sa, NULL); // Capture du kill poli de Cerbère

    // 1. Initialisation des buffers d'I/O non-bloquants
    atlas_init_io_buffers();

    // 2. Extraction et typage des descripteurs système injectés par Cerbère
    atlas_validate_arguments(argc, argv, &shm_control_name, &event_fd);

    // 3. Projection virtuelle de la forteresse RAM SHM
    SharedMemoryPoolHeader *global_pool = atlas_map_shared_memory(shm_control_name);

    // 4. Lancement de la turbine événementielle globale
    atlas_run(global_pool, event_fd);

    return EXIT_SUCCESS;
}
