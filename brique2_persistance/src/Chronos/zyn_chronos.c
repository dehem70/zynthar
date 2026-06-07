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


#include <zynthar.h>

#include "zyn_chronos.h"

#define RECV_BUFFER_SIZE 8 /* Taille exacte de ChunkRequestPacket */
// Connexion d'autorité unique gérée par Chronos
static sqlite3 *g_db_main = NULL;
static volatile sig_atomic_t g_shutdown_requested = 0;

void handle_shutdown_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        g_shutdown_requested = 1;
    }
}

int chronos_init(void) {
    char path_world[1024];
    char path_river[1024];
    char path_delta[1024];
    int rc;

    // 1. Reconstruction des chemins sur le Ramdisk
    snprintf(path_world, sizeof(path_world), "%s/%s", ZYN_RAMDISK_PATH, ZYN_DB_WORLD);
    snprintf(path_river, sizeof(path_river), "%s/%s", ZYN_RAMDISK_PATH, ZYN_DB_RIVER);
    snprintf(path_delta, sizeof(path_delta), "%s/%s", ZYN_RAMDISK_PATH, ZYN_DB_DELTA);

    fprintf(stdout, "[⏳ CHRONOS] Ouverture de la base maîtresse : %s\n", path_world);
    
    // 2. Ouverture de la base principale (Relief)
    rc = sqlite3_open_v2(path_world, &g_db_main, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[❌ CHRONOS] Erreur ouverture base principale : %s\n", sqlite3_errmsg(g_db_main));
        return -1;
    }

    // 3. Unification de l'espace de nommage via ATTACH DATABASE
    char attach_query[2048];
    snprintf(attach_query, sizeof(attach_query), "ATTACH DATABASE '%s' AS rivers;", path_river);
    rc = sqlite3_exec(g_db_main, attach_query, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[❌ CHRONOS] Échec de l'ATTACH de la base rivières : %s\n", sqlite3_errmsg(g_db_main));
        sqlite3_close(g_db_main);
        return -1;
    }
    fprintf(stdout, "[🔗 CHRONOS] Base rivières rattachée avec succès.\n");

/*    snprintf(attach_query, sizeof(attach_query), "ATTACH DATABASE '%s' AS deltas;", path_delta);
    rc = sqlite3_exec(g_db_main, attach_query, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[❌ CHRONOS] Échec de l'ATTACH de la base deltas : %s\n", sqlite3_errmsg(g_db_main));
        sqlite3_close(g_db_main);
        return -1;
    }
    fprintf(stdout, "[🔗 CHRONOS] Base deltas rattachée avec succès.\n");  */

    // 4. Injection des PRAGMA de compétition pour la performance pure
    const char *pragmas = 
        "PRAGMA journal_mode = WAL;"
        "PRAGMA synchronous = NORMAL;"
        "PRAGMA temp_store = MEMORY;"
        "PRAGMA mmap_size = 30000000;" // Allocation mmap de 30 Mo pour court-circuiter le kernel
        "PRAGMA journal_size_limit = 5242880;";

    rc = sqlite3_exec(g_db_main, pragmas, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[⚠️ CHRONOS] Attention, certains PRAGMA ont été refusés : %s\n", sqlite3_errmsg(g_db_main));
    }

    fprintf(stdout, "[🟢 CHRONOS] SQLite3 configuré en mode haute performance sur le Ramdisk.\n");
    return 0;
}

void chronos_shutdown(void) {
    if (g_db_main) {
        fprintf(stdout, "[🛑 CHRONOS] Fermeture des connexions SQLite3...\n");
        sqlite3_close(g_db_main);
        g_db_main = NULL;
    }
}

/**
 * @brief Point d'entrée principal pour la version basique de test de Chronos.
 */
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    uint8_t buffer[RECV_BUFFER_SIZE];
    
    // Configuration des handlers de signaux POSIX
    struct sigaction sa;
    sa.sa_handler = handle_shutdown_signal;
    sigemptyset(&sa.sa_mask);
    
    // CRITIQUE : Aucun drapeau (surtout pas SA_RESTART). 
    // Cela force TOUS les appels système bloquants (accept, read) à échouer immédiatement avec EINTR lors d'un signal.
    sa.sa_flags = 0; 
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("=== ZYNTHAR v0.1 - SERVEUR CHRONOS (WORKER 0) ===\n");
    
    // Lancement du cycle d'ouverture
    if (chronos_init() != 0) {
        fprintf(stderr, "[❌ CHRONOS] Échec de l'initialisation de la base de données. Cerbère est-il lancé ?\n");
        return EXIT_FAILURE;
    }
    
    printf("[*] Initialisation du canal réseau I/O...\n");

    // 1. Création de la socket TCP brute
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("[❌ CHRONOS] Échec de la création de la socket");
        exit(EXIT_FAILURE);
    }

    // 2. Configuration des options de la socket (Réutilisation rapide du port)
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("[❌ CHRONOS] Échec de setsockopt (SO_REUSEADDR)");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Écoute sur toutes les interfaces, incluant localhost
    address.sin_port = htons(ZYN_CHRONOS_PORT);

    // 3. Liaison de la socket au port 6969
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("[❌ CHRONOS] Échec du bind sur le port");
        exit(EXIT_FAILURE);
    }

    // 4. Passage en mode écoute (Backlog de 10 connexions simultanées pour le moment)
    if (listen(server_fd, 10) < 0) {
        perror("[❌ CHRONOS] Échec du listen");
        exit(EXIT_FAILURE);
    }

    printf("[+] Chronos est en ligne et écoute sur le port %d\n\n", ZYN_CHRONOS_PORT);
    
    fprintf(stdout, "[✅ CHRONOS] Initialisation et configurations validées.\n");

    // Boucle d'écoute infinie (réactive)
    while (!g_shutdown_requested) {
        
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        
        if (new_socket < 0) {
            if (errno == EINTR) break; // Sortie propre si Ctrl+C pendant l'accept
            perror("[❌ CHRONOS] Échec accept");
            continue;
        }

        printf("[✅ CHRONOS] Client B3 connecté. Ouverture du tunnel persistant.\n");

        // BOUCLE 2 : Flux continu sur la même socket (Connexion permanente)
        while (!g_shutdown_requested) {
            // Lecture stricte des 8 octets attendus
            ssize_t bytes_read = read(new_socket, buffer, RECV_BUFFER_SIZE);

            if (bytes_read == RECV_BUFFER_SIZE) {
                // Traitement direct (Zéro-Copy)
                ChunkRequestPacket *packet = (ChunkRequestPacket *)buffer;
                MacroKey key;
                key.id = ntohl(packet->macro_chunk_id);

                printf("[✅ CHRONOS] REQUÊTE : Macro[%d, %d] | Micro[%u, %u, %u] | LOD %u\n", 
                       key.coord.x, key.coord.z, packet->mc_x, packet->mc_y, packet->mc_z, packet->lod);

                // Simulation réponse RLE
                uint8_t fake_rle_data[] = { 0xAA, 0xBB, 0x05, 0x00, 0x12, 0x34, 0x56, 0x78, 0x00, 0xFF, 0xEE, 0xDD };
                if (send(new_socket, fake_rle_data, sizeof(fake_rle_data), 0) < 0) {
                    perror("[❌ CHRONOS] Erreur d'envoi, déconnexion client");
                    break; 
                }

            } else if (bytes_read == 0) {
                // Déconnexion propre ou Ctrl+C du simulateur
                printf("[❌ CHRONOS] Le client B3 a fermé le tunnel.\n");
                break; 
            } else if (bytes_read > 0) {
                printf("[❌ CHRONOS] Paquet corrompu ou fragmenté. Flush du flux.\n");
                break;
            } else {
                // Erreur de lecture sur la socket
                if (errno == EINTR) {
                    // Le signal a interrompu le read(). On quitte la boucle interne immédiatement.
                    printf("\n[❌ CHRONOS] Read interrompu par le signal d'arrêt.\n");
                    break;
                }
                perror("[❌ CHRONOS] Erreur lecture socket");
                break;
            }
        }

        close(new_socket);
        new_socket = -1;
        printf("[✅ CHRONOS] Tunnel clos. Retour en attente d'un nouveau client.\n\n");
    }

    close(server_fd);
    chronos_shutdown();
    fprintf(stdout, "[💤 CHRONOS] Fin d'exécution.\n");

    return EXIT_SUCCESS;
}
 
