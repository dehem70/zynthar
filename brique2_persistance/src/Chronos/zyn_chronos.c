/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 07/06/2026
 *
 * zyn_chronos  :
 * utilisation :
 *
 * =============================================================================*/
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

// En-têtes requis sous Linux/POSIX pour manipuler 'struct stat' et 'mkdir'
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdint.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <mqueue.h>

#include <zynthar.h>

#include "zyn_chronos.h"
#include "zyn_chronos_utils.h"



int main(int argc, char *argv[]) {
    // Configuration des buffers de sortie pour éviter la mise en tampon des logs
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    // 1. Gestion des signaux système (SIGINT / SIGTERM)
    struct sigaction sa;
    sa.sa_handler = handle_shutdown_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // 2. Initialisation technique et parsing des arguments
    char *shm_name = NULL;
    chronos_parse_args(argc, argv, &shm_name);

    // 3. Attachement à l'infrastructure partagée
    SharedMemoryPoolHeader *global_pool = chronos_attach_shm(shm_name);

    // 4. Initialisation de la plomberie réseau
    struct sockaddr_in address;
    int addrlen;
    int server_fd = chronos_create_server_socket(&address, &addrlen);

    // 5. Ouverture de la file Atropos globale (Chronos écrit dedans)
    mqd_t atropos_mq = mq_open(ZYN_ATROPOS_MQ_NAME, O_WRONLY);
    if (atropos_mq == (mqd_t)-1) {
        perror("[❌ CHRONOS] Impossible de se lier à la MQ globale d'Atropos");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 6. Lancement de la boucle applicative principale (Boucle de Session)
    chronos_run(global_pool, server_fd, address, addrlen, atropos_mq);

    // 7. Extinction et Nettoyage de sortie
    close(server_fd);
    mq_close(atropos_mq);

    printf("[✅ CHRONOS] Processus éteint proprement à 100%%.\n");
    return EXIT_SUCCESS;
}
