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

#define RECV_BUFFER_SIZE 8 /* Taille exacte de ChunkRequestPacket */
// Connexion d'autorité unique gérée par Chronos
static sqlite3 *g_db_main = NULL;
static volatile sig_atomic_t g_shutdown_requested = 0;

void handle_shutdown_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        g_shutdown_requested = 1;
    }
}


/**
 * @brief Pioche un nœud libre dans le pool partagé de manière bloquante / adaptative.
 * @return L'index du nœud pioché (0 à MAX_POOL_PAGES-1).
 */
static int32_t chronos_pop_shm_context(SharedMemoryPoolHeader *pool) {
    pthread_mutex_lock(&pool->lock);
    
    // Si la LIFO est vide, Chronos s'endort sur la variable de condition partagée
    while (pool->top_idx == -1) {
        printf("[⏳ CHRONOS] Pool vide ! Backpressure activé, augmentation du seuil de sécurité.\n");
        pool->low_watermark += 2; // Signal indirect à Cerbère d'allouer plus grand
        
        pthread_cond_wait(&pool->cond_free, &pool->lock);
    }
    
    // Extraction de l'index du sommet
    int32_t pioched_idx = pool->top_idx;
    pool->top_idx = pool->nodes[pioched_idx].next_free_idx;
    
    // Nettoyage du lien pour des raisons de sécurité
    pool->nodes[pioched_idx].next_free_idx = -1;
    pool->current_count--;
    
    pthread_mutex_unlock(&pool->lock);
    return pioched_idx;
}

/**
 * @brief Récupère ou associe l'adresse virtuelle locale de la page de 16 Mo spécifiée par l'index.
 */
static uint8_t* chronos_get_and_map_page(SharedMemoryPoolHeader *pool, int32_t idx) {
    // Cache local persistant dans le processus de Chronos
    static uint8_t* local_page_cache[MAX_POOL_PAGES] = {NULL};
    
    if (local_page_cache[idx] != NULL) {
        return local_page_cache[idx]; // Cache hit ! Évite un mmap coûteux.
    }
    
    // Cache miss : On attache le fichier de 16 Mo pour la première fois
    int page_fd = shm_open(pool->nodes[idx].context.shm_page_name, O_RDWR, 0666);
    if (page_fd == -1) {
        perror("[❌ CHRONOS] Impossible d'ouvrir la page de 16 Mo nommée");
        return NULL;
    }
    
    uint8_t *mapped_ptr = (uint8_t*)mmap(NULL, PAGE_SIZE_16MO, PROT_READ | PROT_WRITE, MAP_SHARED, page_fd, 0);
    close(page_fd); // Inutile après le mmap
    
    if (mapped_ptr == MAP_FAILED) {
        perror("[❌ CHRONOS] Échec mmap local pour la page de 16 Mo");
        return NULL;
    }
    
    local_page_cache[idx] = mapped_ptr;
    return mapped_ptr;
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

void chronos_run(SharedMemoryPoolHeader *pool,int server_fd,struct sockaddr_in address,int addrlen,mqd_t atropos_mq) {
    uint8_t buffer[RECV_BUFFER_SIZE];
    int new_socket;
    printf("[⏳ CHRONOS] Démarrage de la boucle applicative...\n");

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
        return ;
    }

    // 3. Unification de l'espace de nommage via ATTACH DATABASE
    char attach_query[2048];
    snprintf(attach_query, sizeof(attach_query), "ATTACH DATABASE '%s' AS rivers;", path_river);
    rc = sqlite3_exec(g_db_main, attach_query, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[❌ CHRONOS] Échec de l'ATTACH de la base rivières : %s\n", sqlite3_errmsg(g_db_main));
        sqlite3_close(g_db_main);
        return ;
    }
    fprintf(stdout, "[🔗 CHRONOS] Base rivières rattachée avec succès.\n");

/*    snprintf(attach_query, sizeof(attach_query), "ATTACH DATABASE '%s' AS deltas;", path_delta);
    rc = sqlite3_exec(g_db_main, attach_query, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[❌ CHRONOS] Échec de l'ATTACH de la base deltas : %s\n", sqlite3_errmsg(g_db_main));
        sqlite3_close(g_db_main);
        return ;
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

    // Préparation de la requête optimisée à 1 seule ligne (grâce à tes outils Brique 5 mis à jour !)
    const char *sql = "SELECT data FROM macro_chunks WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db_main, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[❌ CHRONOS] Échec de préparation de la requête SQL\n");
        sqlite3_close(g_db_main);
        return;
    }

    // 2. Boucle principale de consommation (Attente des demandes de la Brique 1)
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
                Id key;
                key.id = ntohl(packet->macro_chunk_id);
                
               

                printf("[✅ CHRONOS] REQUÊTE : id %d Macro[%d, %d] | Micro[%u, %u, %u] | LOD %u\n", 
                       key.id,key.x+256*key.rx, key.z+256*key.rz, packet->mc_x, packet->mc_y, packet->mc_z, packet->lod);
                       
                // B. Pioche d'un contexte libre dans la SHM de Cerbère (Bloquant si vide)
                int32_t idx = chronos_pop_shm_context(pool);
        
                // C. Mappage ou récupération du pointeur physique de 16 Mo via le cache local
                uint8_t *voxels_page = chronos_get_and_map_page(pool, idx);
        
                if (!voxels_page) {
                    // Sécurité si le mmap échoue
                    continue;
                }
                
                // remplissage des données venant de la requete
                pool->nodes[idx].context.lod=packet->lod;
                pool->nodes[idx].context.mc_x=packet->mc_x;
                pool->nodes[idx].context.mc_y=packet->mc_y;
                pool->nodes[idx].context.mc_z=packet->mc_z;
                

                // D. Extraction Éclair depuis SQLite3
                sqlite3_bind_int64(stmt, 1, key.id);
        
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    const void *blob_ptr = sqlite3_column_blob(stmt, 0);
                    int blob_size = sqlite3_column_bytes(stmt, 0);
                    
                    if (blob_ptr && blob_size >= sizeof(MacroChunk_db)) {
                        const MacroChunk_db *macro_data = (const MacroChunk_db *)blob_ptr;
                        
                        // Lecture directe de la ligne unique
                        pool->nodes[idx].context.biome = macro_data->biome;
                        pool->nodes[idx].context.coin_nw = macro_data->elevation_coin_nw;
                        pool->nodes[idx].context.coin_ne = macro_data->elevation_coin_ne;
                        pool->nodes[idx].context.coin_sw = macro_data->elevation_coin_sw;
                        pool->nodes[idx].context.coin_se = macro_data->elevation_coin_se;
                        pool->nodes[idx].context.temperature_raw=macro_data->temperature_raw;
                    } else {
                        // Cas où le BLOB en base est corrompu ou n'a pas la bonne taille
                        fprintf(stderr, "[⚠️ CHRONOS] BLOB invalide ou corrompu pour macro_id %ld\n", (long)key.id);
                        pool->nodes[idx].context.biome   = 0;
                        pool->nodes[idx].context.coin_nw = 0;
                        pool->nodes[idx].context.coin_ne = 0;
                        pool->nodes[idx].context.coin_sw = 0;
                        pool->nodes[idx].context.coin_se = 0;
                        pool->nodes[idx].context.temperature_raw=0;
                    }
                    // E. Écriture des deltas historiques par Chronos dans la page de 16 Mo
                    // voxels_page[index] = ... (tes modifications/constructions historiques)
            
                } else {
                    // Macro-chunk introuvable (océan vide ou coordonnées hors limites)
                    pool->nodes[idx].context.biome = 0;
                    pool->nodes[idx].context.coin_nw = 0;
                    pool->nodes[idx].context.coin_ne = 0;
                    pool->nodes[idx].context.coin_sw = 0;
                    pool->nodes[idx].context.coin_se = 0;
                    pool->nodes[idx].context.temperature_raw=0;
                }
        
                sqlite3_reset(stmt);

                // F. PASSAGE DU FLAMBEAU À ATROPOS
                // Remplir les coordonnées pour le découpage d'Atropos
                pool->nodes[idx].context.macro_id = key.id;
                pool->nodes[idx].context.context_id = idx;
        
                AtroposMessage msg;
                msg.shm_node_idx = idx; // On transmet l'index de la page (0 à 63)

                // mq_send prend : le descripteur, le pointeur du message, sa taille, et la priorité (0 par défaut)
                if (mq_send(atropos_mq, (const char *)&msg, sizeof(AtroposMessage), 0) == -1) {
                    // Si l'envoi a échoué pour une autre raison qu'une file pleine (ex: interruption par un signal)
                    // On met en place une petite boucle de secours (retry)
                    int retries = 0;
                    while (mq_send(atropos_mq, (const char *)&msg, sizeof(AtroposMessage), 0) == -1 && retries < 5) {
                        usleep(1000); // On attend 1 milliseconde avant de réessayer
                        retries++;
                    }
    
                    if (retries >= 5) {
                        fprintf(stderr, "[❌ CRITIQUE] Chronos n'a pas pu notifier Atropos pour la page %d après 5 essais !\n", idx);
                        // Ici, on pourrait décider de libérer le nœud ou de lever une alerte majeure pour Cerbère
                    }
                } else {
                    printf("[⏳ CHRONOS] Page %d chargée en RAM. Signal envoyé à Atropos.\n", idx);
                }
                                
/*                printf("[⏳ CHRONOS]        Job %d ! Macro_ID : %d | Micro[%u, %u, %u] | LOD %u - Biome : %u - température : %.2f - coin [ %d,%d,%d,%d]\n", 
                       pool->nodes[idx].context.context_id,
                       pool->nodes[idx].context.macro_id,
                       pool->nodes[idx].context.mc_x,
                       pool->nodes[idx].context.mc_y,
                       pool->nodes[idx].context.mc_z,
                       pool->nodes[idx].context.lod,
                       pool->nodes[idx].context.biome,
                       RAW_TO_FLOAT(pool->nodes[idx].context.temperature_raw),
                       pool->nodes[idx].context.coin_ne,
                       pool->nodes[idx].context.coin_se,
                       pool->nodes[idx].context.coin_sw,
                       pool->nodes[idx].context.coin_nw);
*/
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
        
        
    // Nettoyage si on sort de la boucle (théoriquement jamais)
    sqlite3_finalize(stmt);
    sqlite3_close(g_db_main);
}


/**
 * @brief Point d'entrée principal pour la version basique de test de Chronos.
 */
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
        
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
    
    // 1. Vérification que Cerbère nous a bien transmis le nom du segment maître
    if (argc < 2) {
        fprintf(stderr, "[❌ CHRONOS] Erreur critique : Nom du segment de contrôle SHM manquant en argument.\n");
        return EXIT_FAILURE;
    }
    
    char *shm_control_name = argv[1];
    
    // 2. Connexion au segment de contrôle déjà créé par Cerbère
    int ctrl_fd = shm_open(shm_control_name, O_RDWR, 0666);
    if (ctrl_fd == -1) {
        perror("[❌ CHRONOS] Erreur critique lors de la connexion au segment de contrôle maître");
        return EXIT_FAILURE;
    }
    
    SharedMemoryPoolHeader *global_pool = (SharedMemoryPoolHeader*)mmap(
        NULL, sizeof(SharedMemoryPoolHeader), PROT_READ | PROT_WRITE, MAP_SHARED, ctrl_fd, 0
    );
    close(ctrl_fd);
    
    if (global_pool == MAP_FAILED) {
        perror("[❌ CHRONOS] Échec du mmap sur la structure de contrôle globale");
        return EXIT_FAILURE;
    }
    
    // Lancement du cycle d'ouverture
/*    if (chronos_init() != 0) {
        fprintf(stderr, "[❌ CHRONOS] Échec de l'initialisation de la base de données. Cerbère est-il lancé ?\n");
        return EXIT_FAILURE;
    }
  */  
  
  // 🎯 CONNEXION À LA BOÎTE AUX LETTRES D'ATROPOS
    printf("[⏳ CHRONOS] Connexion à la Message Queue d'Atropos...\n");
    mqd_t atropos_mq = mq_open(ZYN_ATROPOS_MQ_NAME, O_WRONLY);
    if (atropos_mq == (mqd_t)-1) {
        perror("[❌ CHRONOS] Erreur critique : Impossible de se connecter à la Message Queue");
        return EXIT_FAILURE;
    }

    printf("[✅ CHRONOS] Pipeline Atropos & MQ connectés.\n");
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

    // 3. Lancement de la boucle applicative principale
    chronos_run(global_pool,server_fd,address,addrlen,atropos_mq);
    close(server_fd);
    chronos_shutdown();
    mq_close(atropos_mq);
    fprintf(stdout, "[💤 CHRONOS] Fin d'exécution.\n");

    return EXIT_SUCCESS;
}
 
