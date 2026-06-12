/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 12/06/2026
 *
 * zyn_atlas_utils  :
 * utilisation :
 *
 * =============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <mqueue.h>

#include <zynthar.h>
#include "zyn_atlas_utils.h"

void atlas_init_io_buffers(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
}

void atlas_validate_arguments(int argc, char *argv[], char **shm_name, int *event_fd) {
    if (argc < 3) {
        fprintf(stderr, "[❌ ATLAS] Erreur critique : Arguments manquants (SHM + EventFD requis).\n");
        exit(EXIT_FAILURE);
    }
    *shm_name = argv[1];
    *event_fd = atoi(argv[2]);
}

SharedMemoryPoolHeader* atlas_map_shared_memory(const char *shm_name) {
    int ctrl_fd = shm_open(shm_name, O_RDWR, 0666);
    if (ctrl_fd == -1) {
        perror("[❌ ATLAS] Erreur de connexion au segment SHM");
        exit(EXIT_FAILURE);
    }

    SharedMemoryPoolHeader *pool = (SharedMemoryPoolHeader*)mmap(
        NULL, sizeof(SharedMemoryPoolHeader), PROT_READ | PROT_WRITE, MAP_SHARED, ctrl_fd, 0
    );
    close(ctrl_fd);

    if (pool == MAP_FAILED) {
        perror("[❌ ATLAS] Échec critique du mmap global");
        exit(EXIT_FAILURE);
    }
    return pool;
}

mqd_t atlas_open_chronos_queue(void) {
    mqd_t chronos_mq = mq_open(ZYN_CHRONOS_RECV_MQ_NAME, O_WRONLY);
    if (chronos_mq == (mqd_t)-1) {
        perror("[❌ ATLAS] Échec d'ouverture de la MQ de retour Chronos");
        exit(EXIT_FAILURE);
    }
    return chronos_mq;
}

int atlas_setup_epoll(int event_fd) {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("[❌ ATLAS] Échec de la création de l'instance epoll");
        exit(EXIT_FAILURE);
    }

    struct epoll_event ev;
    ev.events = EPOLLIN; // Réveil sur niveau de données disponibles (EventFD signé)
    ev.data.fd = event_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event_fd, &ev) == -1) {
        perror("[❌ ATLAS] Échec de l'association de l'eventfd à epoll");
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    return epoll_fd;
}

void atlas_compress_and_signal_page(SharedMemoryPoolHeader *pool, int32_t idx, mqd_t chronos_mq) {
    // 🗜️ LOGIQUE DE COMPRESSION (Actuellement simulée "sur place" à 512 Ko)
    // C'est ici que s'interfacera l'appel à la future brique RLE !
    pool->nodes[idx].context.compressed_size = 512 * 1024; 

#if ZYN_LOG_DEBUG
    printf("[🌍 ATLAS] Page %d compressée avec succès (%d octets).\n", idx, pool->nodes[idx].context.compressed_size);
#endif

    // 🏁 BASCULEMENT DE L'INTERRUPTEUR DE RETOUR (Cast sécurisé pour IWYU/Clang)
    __atomic_store_n((uint8_t *)&pool->nodes[idx].context.status, ZYN_STATUS_COMPRESSED, __ATOMIC_RELEASE);
    
    // Notification asynchrone renvoyée à la turbine d'écriture de Chronos
    int node_idx = idx;
    if (mq_send(chronos_mq, (const char*)&node_idx, sizeof(node_idx), 0) == -1) {
        fprintf(stderr, "[⚠️ ATLAS] Échec mq_send pour le nœud %d (Queue pleine ?)\n", idx);
    }
}
