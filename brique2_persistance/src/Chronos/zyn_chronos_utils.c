/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 12/06/2026
 *
 * zyn_chronos_utils  :
 * utilisation :
 *
 * =============================================================================*/
  
#include <zynthar.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <signal.h>
#include "zyn_chronos_utils.h"
#include "zyn_chronos.h"

// Drapeau d'arrêt externe partagé (déclaré dans zyn_chronos.c)
extern volatile sig_atomic_t g_shutdown_requested;

void chronos_init_io_buffers(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
}

void chronos_validate_arguments(int argc, char *argv[], char **shm_name) {
    if (argc < 2) {
        fprintf(stderr, "[❌ CHRONOS] Erreur critique : Nom du segment SHM manquant.\n");
        exit(EXIT_FAILURE);
    }
    *shm_name = argv[1];
}

SharedMemoryPoolHeader* chronos_map_shared_memory(const char *shm_name) {
    int ctrl_fd = shm_open(shm_name, O_RDWR, 0666);
    if (ctrl_fd == -1) {
        perror("[❌ CHRONOS] Erreur de connexion au segment SHM");
        exit(EXIT_FAILURE);
    }
    SharedMemoryPoolHeader *pool = (SharedMemoryPoolHeader*)mmap(
        NULL, sizeof(SharedMemoryPoolHeader), PROT_READ | PROT_WRITE, MAP_SHARED, ctrl_fd, 0
    );
    close(ctrl_fd);
    if (pool == MAP_FAILED) {
        perror("[❌ CHRONOS] Échec du mmap global");
        exit(EXIT_FAILURE);
    }
    return pool;
}

sqlite3* chronos_init_database(void) {
    // 1. Validation de l'arborescence physique
    struct stat st = {0};
    if (stat("world", &st) == -1) {
        if (mkdir("world", 0777) == -1) {
            perror("[❌ CHRONOS] Impossible de créer le répertoire 'world'");
            exit(EXIT_FAILURE);
        }
    }

    // 2. Ouverture de la base maître
    sqlite3 *db = NULL;
    int rc = sqlite3_open("world/zyn-main.db", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[❌ CHRONOS] Échec d'ouverture SQLite : %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }
    printf("[💾 CHRONOS] Connexion d'autorité à 'world/zyn-main.db' établie.\n");

    // 3. Rafale de PRAGMAs Haute Performance
    char *zErrMsg = NULL;
    const char* pragmas[] = {
        "PRAGMA journal_mode=WAL;",
        "PRAGMA synchronous=NORMAL;",
        "PRAGMA cache_size=-64000;", // ~64 Mo de cache RAM dédié
        "PRAGMA locking_mode=EXCLUSIVE;",
        "PRAGMA temp_store=MEMORY;"
    };

    for (int i = 0; i < 5; i++) {
        rc = sqlite3_exec(db, pragmas[i], NULL, NULL, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[⚠️ CHRONOS] SQL error sur %s : %s\n", pragmas[i], zErrMsg);
            sqlite3_free(zErrMsg);
        }
    }
    return db;
}

int chronos_setup_server_socket(void) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    if ((server_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0)) == 0) {
        perror("[❌ CHRONOS] Échec de la création de la socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("[❌ CHRONOS] Échec de setsockopt (SO_REUSEADDR)");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(ZYN_CHRONOS_PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("[❌ CHRONOS] Échec du bind sur le port 6969");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("[❌ CHRONOS] Échec du listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("[+] Chronos en ligne et à l'écoute sur le port %d\n\n", ZYN_CHRONOS_PORT);
    return server_fd;
}

void* chronos_reader_thread(void *arg) {
    ThreadArgs *args = (ThreadArgs*)arg;
    ChunkRequestPacket pkt; 
    size_t packet_size = sizeof(ChunkRequestPacket);

    printf("[🌀 CHRONOS-READER] Thread d'écoute réseau initialisé (Restauration de la logique SHM).\n");

    while (!g_shutdown_requested) {
        // 1. Lecture de la trame géométrique du client (8 octets stricts)
        ssize_t bytes_read = read(args->client_socket, &pkt, packet_size);
        
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            perror("[❌ CHRONOS-READER] Erreur de lecture socket");
            break;
        }
        
        if (bytes_read == 0) {
            printf("[🌀 CHRONOS-READER] Déconnexion du simulateur détectée (EOF).\n");
            int poison = -1;
            mq_send(args->recv_mq, (const char*)&poison, sizeof(poison), 0);
            break;
        }

        if (bytes_read == (ssize_t)packet_size) {
            // 2. 🎯 PIOCHE DU CONTEXTE LIBRE (Ta logique d'origine !)
            // On demande à la SHM de nous donner un index de page disponible (Bloquant si plein)
            int32_t idx = chronos_pop_shm_context(args->pool);

            // 3. Sécurité de bornes par rapport au pool de calcul
            if (idx < 0 || idx >= MAX_POOL_PAGES) {
                fprintf(stderr, "[❌ CHRONOS-READER] Erreur critique : L'index retourné par le POP est invalide (%d).\n", idx);
                continue;
            }

            // 4. Copie des métadonnées géométriques reçues du réseau dans le contexte de la page SHM allouée
            args->pool->nodes[idx].context.macro_id = pkt.macro_chunk_id;
            args->pool->nodes[idx].context.mc_x     = pkt.mc_x;
            args->pool->nodes[idx].context.mc_y     = pkt.mc_y;
            args->pool->nodes[idx].context.mc_z     = pkt.mc_z;
            args->pool->nodes[idx].context.lod      = pkt.lod;

#if ZYN_LOG_DEBUG
            printf("[🌀 CHRONOS-READER] Paquet associé à la page SHM Index: %d (MacroID: %u)\n", idx, pkt.macro_chunk_id);
#endif

            // 5. Activation atomique pour signaler aux Forgerons que la page est prête à être calculée
            __atomic_store_n((uint8_t *)&args->pool->nodes[idx].context.status, ZYN_STATUS_COMPUTING, __ATOMIC_RELEASE);
        }
    }
    return NULL;
}
void* chronos_writer_thread(void *arg) {
    ThreadArgs *args = (ThreadArgs*)arg;
    int32_t node_idx_received = 0;
    unsigned int prio;

    printf("[🚀 CHRONOS-WRITER] Thread d'expédition réseau initialisé.\n");

    while (1) {
        if (mq_receive(args->recv_mq, (char *)&node_idx_received, sizeof(node_idx_received), &prio) == -1) {
            if (g_shutdown_requested) break;
            perror("[❌ CHRONOS-WRITER] Erreur mq_receive");
            continue;
        }

        if (node_idx_received == -1 || g_shutdown_requested) {
            printf("[🚀 CHRONOS-WRITER] Signal d'évacuation intercepté. Sortie.\n");
            break;
        }

        uint8_t status = __atomic_load_n((uint8_t *)&args->pool->nodes[node_idx_received].context.status, __ATOMIC_ACQUIRE);

        if (status == ZYN_STATUS_COMPRESSED) {
            uint32_t payload_size = args->pool->nodes[node_idx_received].context.compressed_size;
            const char *page_name = args->pool->nodes[node_idx_received].context.shm_page_name;

            // 🛠️ OUVERTURE DU FICHIER SHM
            int page_fd = shm_open(page_name, O_RDONLY, 0666);
            if (page_fd == -1) {
                fprintf(stderr, "[❌ CHRONOS-WRITER] Impossible d'ouvrir la page SHM %s\n", page_name);
                continue;
            }

            // 🎯 SÉCURITÉ ABSOLUE : On interroge Linux sur la taille RÉELLE du segment
            struct stat page_stat;
            if (fstat(page_fd, &page_stat) == -1) {
                perror("[❌ CHRONOS-WRITER] Échec du fstat sur la page SHM");
                close(page_fd);
                continue;
            }
            size_t page_size = page_stat.st_size; // Taille exacte en octets (plus de constante magique !)

            // On projette la taille exacte pour interdire tout dépassement
            uint8_t *payload_ptr = (uint8_t *)mmap(NULL, page_size, PROT_READ, MAP_SHARED, page_fd, 0);
            close(page_fd);

            if (payload_ptr == MAP_FAILED) {
                perror("[❌ CHRONOS-WRITER] Échec du mmap de la page de données");
                continue;
            }

            // Envoi de l'en-tête de taille réseau (4 octets)
            ssize_t sent = write(args->client_socket, &payload_size, 4);
            if (sent > 0) {
                uint32_t total_sent = 0;
                while (total_sent < payload_size && !g_shutdown_requested) {
                    uint32_t to_send = payload_size - total_sent;
                    if (to_send > 65536) to_send = 65536; 

                    // On vérifie une double sécurité : ne jamais lire hors du mmap réel
                    if (total_sent + to_send > page_size) {
                        to_send = page_size - total_sent;
                    }
                    if (to_send == 0) break;

                    ssize_t b_sent = write(args->client_socket, payload_ptr + total_sent, to_send);
                    if (b_sent < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            usleep(10);
                            continue;
                        }
                        break;
                    }
                    if (b_sent == 0) break;
                    total_sent += b_sent;
                }
#if ZYN_LOG_DEBUG
                printf("[🚀 CHRONOS-WRITER] Page %s (%d) envoyée avec succès (%u octets).\n", page_name, node_idx_received, total_sent);
#endif
            }

            // Libération de la projection avec la taille dynamique exacte
            munmap(payload_ptr, page_size);

            // RESET ET LIBÉRATION
            __atomic_store_n((uint8_t *)&args->pool->nodes[node_idx_received].context.status, ZYN_STATUS_FREE, __ATOMIC_RELEASE);
        }
    }
    return NULL;
}
