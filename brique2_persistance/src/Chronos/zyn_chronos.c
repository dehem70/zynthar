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
void* chronos_reader_thread(void *arg) {
    ChronosSessionArgs *args = (ChronosSessionArgs *)arg;
    uint8_t buffer[RECV_BUFFER_SIZE];

    printf("[📥 CHRONOS-READER] Turbine d'entrée activée (Tunnel persistant).\n");

    // BOUCLE INTERNE : Flux continu sur la même socket (Connexion permanente)
    while (!g_shutdown_requested) {
        // Lecture stricte des 8 octets attendus
        ssize_t bytes_read = read(args->socket_fd, buffer, RECV_BUFFER_SIZE);

        if (bytes_read == RECV_BUFFER_SIZE) {
            // Traitement direct (Zéro-Copy)
            ChunkRequestPacket *packet = (ChunkRequestPacket *)buffer;
            Id key;
            key.id = ntohl(packet->macro_chunk_id);

            printf("[✅ READER] REQUÊTE : id %d Macro[%d, %d] | Micro[%u, %u, %u] | LOD %u\n", 
                   key.id, key.x + 256 * key.rx, key.z + 256 * key.rz, 
                   packet->mc_x, packet->mc_y, packet->mc_z, packet->lod);

            // B. Pioche d'un contexte libre dans la SHM de Cerbère (Bloquant si vide)
            int32_t idx = chronos_pop_shm_context(args->pool);

            // C. Mappage ou récupération du pointeur physique de 16 Mo via le cache local
            uint8_t *voxels_page = chronos_get_and_map_page(args->pool, idx);

            if (!voxels_page) {
                // Sécurité si le mmap échoue, on continue pour ne pas tuer la session
                continue;
            }

            // Remplissage des données venant de la requête
            args->pool->nodes[idx].context.lod = packet->lod;
            args->pool->nodes[idx].context.mc_x = packet->mc_x;
            args->pool->nodes[idx].context.mc_y = packet->mc_y;
            args->pool->nodes[idx].context.mc_z = packet->mc_z;

            // D. Extraction Éclair depuis SQLite3 (en utilisant le stmt partagé)
            sqlite3_bind_int64(args->stmt, 1, key.id);

            if (sqlite3_step(args->stmt) == SQLITE_ROW) {
                const void *blob_ptr = sqlite3_column_blob(args->stmt, 0);
                int blob_size = sqlite3_column_bytes(args->stmt, 0);
                
                if (blob_ptr && blob_size >= sizeof(MacroChunk_db)) {
                    const MacroChunk_db *macro_data = (const MacroChunk_db *)blob_ptr;
                    
                    // Lecture directe de la ligne unique
                    args->pool->nodes[idx].context.biome = macro_data->biome;
                    args->pool->nodes[idx].context.coin_nw = macro_data->elevation_coin_nw;
                    args->pool->nodes[idx].context.coin_ne = macro_data->elevation_coin_ne;
                    args->pool->nodes[idx].context.coin_sw = macro_data->elevation_coin_sw;
                    args->pool->nodes[idx].context.coin_se = macro_data->elevation_coin_se;
                    args->pool->nodes[idx].context.temperature_raw = macro_data->temperature_raw;
                } else {
                    // Cas où le BLOB en base est corrompu ou n'a pas la bonne taille
                    fprintf(stderr, "[⚠️ READER] BLOB invalide ou corrompu pour macro_id %ld\n", (long)key.id);
                    args->pool->nodes[idx].context.biome   = 0;
                    args->pool->nodes[idx].context.coin_nw = 0;
                    args->pool->nodes[idx].context.coin_ne = 0;
                    args->pool->nodes[idx].context.coin_sw = 0;
                    args->pool->nodes[idx].context.coin_se = 0;
                    args->pool->nodes[idx].context.temperature_raw = 0;
                }
                // E. Écriture des deltas historiques par Chronos dans la page de 16 Mo
                // voxels_page[index] = ... 
            } else {
                // Macro-chunk introuvable (océan vide ou coordonnées hors limites)
                args->pool->nodes[idx].context.biome = 0;
                args->pool->nodes[idx].context.coin_nw = 0;
                args->pool->nodes[idx].context.coin_ne = 0;
                args->pool->nodes[idx].context.coin_sw = 0;
                args->pool->nodes[idx].context.coin_se = 0;
                args->pool->nodes[idx].context.temperature_raw = 0;
            }

            sqlite3_reset(args->stmt);

            // F. PASSAGE DU FLAMBEAU À ATROPOS
            args->pool->nodes[idx].context.macro_id = key.id;
            args->pool->nodes[idx].context.context_id = idx;
            
            args->pool->nodes[idx].context.jobs_remaining = 4096;
            __atomic_store_n(&args->pool->nodes[idx].context.status, ZYN_STATUS_COMPUTING, __ATOMIC_RELEASE);

            AtroposMessage msg;
            msg.shm_node_idx = idx; // On transmet l'index de la page (0 à 63)

            if (mq_send(args->atropos_mq, (const char *)&msg, sizeof(AtroposMessage), 0) == -1) {
                int retries = 0;
                while (mq_send(args->atropos_mq, (const char *)&msg, sizeof(AtroposMessage), 0) == -1 && retries < 5) {
                    usleep(1000); // On attend 1 milliseconde avant de réessayer
                    retries++;
                }

                if (retries >= 5) {
                    fprintf(stderr, "[❌ CRITIQUE] Reader n'a pas pu notifier Atropos pour la page %d après 5 essais !\n", idx);
                }
            } else {
                printf("[📥 READER] Page %d chargée en RAM. Signal envoyé à Atropos.\n", idx);
            }

            // Et on boucle immédiatement pour lire le read() suivant ! Zéro attente du réseau de retour ici.

        } else if (bytes_read == 0) {
            printf("[❌ READER] Le client B3 a fermé le tunnel.\n");
            g_shutdown_requested = 1; // On lève le drapeau global
            
            // Envoi du jeton de réveil (-1) pour libérer le mq_receive bloquant du Writer
            int poison_pill = -1;
            mq_send(args->recv_mq, (const char*)&poison_pill, sizeof(poison_pill), 0);
            break;
        } else if (bytes_read > 0) {
            printf("[❌ READER] Paquet corrompu ou fragmenté. Flush du flux.\n");
            break;
        } else {
            // Erreur de lecture sur la socket
            if (errno == EINTR) {
                printf("\n[❌ READER] Read interrompu par le signal d'arrêt g_shutdown_requested.\n");
                break;
            }
            perror("[❌ READER] Erreur lecture socket");
            break;
        }
    }

    return NULL;
}

void* chronos_writer_thread(void *arg) {
    ChronosSessionArgs *args = (ChronosSessionArgs *)arg;
    int node_idx_received = -1;

    printf("[📤 CHRONOS-WRITER] Turbine de sortie active (Option B : Anti-Blocking TCP).\n");

    while (!g_shutdown_requested) {
        if (mq_receive(args->recv_mq, (char*)&node_idx_received, sizeof(node_idx_received), NULL) == -1) {
            if (errno == EINTR) continue;
            perror("[❌ WRITER] Erreur mq_receive");
            break;
        }
        
        if (node_idx_received == -1 || g_shutdown_requested) {
            printf("[📤 CHRONOS-WRITER] Jeton de fermeture intercepté. Extinction du thread.\n");
            break;
        }

        uint8_t status = __atomic_load_n(&args->pool->nodes[node_idx_received].context.status, __ATOMIC_ACQUIRE);
        
        if (status == ZYN_STATUS_COMPRESSED) {
            uint32_t size = args->pool->nodes[node_idx_received].context.compressed_size;
            uint64_t macro_id = args->pool->nodes[node_idx_received].context.macro_id;

            uint8_t *voxels_page = chronos_get_and_map_page(args->pool, node_idx_received);
            if (!voxels_page) {
                fprintf(stderr, "[❌ WRITER] Échec critique mmap page %d\n", node_idx_received);
                continue;
            }

            // 🚀 COUCHE OPTION B : Envoi Non-Bloquant avec résilience
            ssize_t total_sent = 0;
            int retries = 0;
            int socket_error = 0;

            while (total_sent < size && !g_shutdown_requested) {
                // MSG_DONTWAIT force Linux à renvoyer EAGAIN au lieu de figer le thread
                ssize_t sent_bytes = send(args->socket_fd, voxels_page + total_sent, size - total_sent, MSG_DONTWAIT);

                if (sent_bytes > 0) {
                    total_sent += sent_bytes;
                    retries = 0; // On a progressé, on réinitialise les essais
                } else if (sent_bytes < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // Le buffer TCP est saturé ! brique3_sim est trop lente.
                        retries++;
                        if (retries > 1000) { 
                            // Si après 1000 micro-pauses ça bloque toujours, la socket est asphyxiée
                            fprintf(stderr, "[⚠️ WRITER] Socket saturée (Backpressure TCP extrême) pour Macro_ID %lu\n", macro_id);
                            // On laisse expirer pour éviter le deadlock total
                            break; 
                        }
                        usleep(10); // Micro-respiration de 10 microsecondes pour laisser le noyau Linux souffler
                    } else {
                        perror("[❌ WRITER] Erreur fatale send");
                        socket_error = 1;
                        break;
                    }
                } else {
                    // Socket close proprement à l'autre bout
                    socket_error = 1;
                    break;
                }
            }

            if (socket_error) {
                break; // On démonte la turbine si le client a déco
            }

            if (total_sent >= size) {
                printf("[♻️ WRITER] BLOB envoyé avec succès pour Macro_ID %lu (%d octets) | Page SHM %d.\n", 
                       macro_id, size, node_idx_received);
            }

            // 🔓 LIBÉRATION DU SLOT MEMOIRE (Quoi qu'il arrive, pour éviter les fuites de pages)
            __atomic_store_n(&args->pool->nodes[node_idx_received].context.status, ZYN_STATUS_FREE, __ATOMIC_RELEASE);
            chronos_push_shm_context(args->pool, node_idx_received);
        }
    }

    return NULL;
}
/**
 * @brief Pioche un nœud libre dans le pool partagé de manière bloquante / adaptative.
 * @return L'index du nœud pioché (0 à MAX_POOL_PAGES-1).
 */
static int32_t chronos_pop_shm_context(SharedMemoryPoolHeader *pool) {
    pthread_mutex_lock(&pool->lock);
    
    // Si la FIFO est vide, Chronos s'endort sur la variable de condition
    while (pool->head_idx == -1) {
        printf("[⏳ CHRONOS] Pool FIFO vide ! Backpressure activé.\n");
        pool->low_watermark += 2; 
        pthread_cond_wait(&pool->cond_free, &pool->lock);
    }
    
    // Extraction de l'index en tête de file
    int32_t pioched_idx = pool->head_idx;
    
    // Déplacement de la tête vers le nœud suivant
    pool->head_idx = pool->nodes[pioched_idx].next_free_idx;
    
    // Si la file devient vide suite au pop, on réinitialise la queue
    if (pool->head_idx == -1) {
        pool->tail_idx = -1;
    }
    
    // Nettoyage du lien de sécurité
    pool->nodes[pioched_idx].next_free_idx = -1;
    pool->current_count--;
    
    pthread_mutex_unlock(&pool->lock);
    return pioched_idx;
}
static void chronos_push_shm_context(SharedMemoryPoolHeader *pool, int32_t free_idx) {
    pthread_mutex_lock(&pool->lock);

    // Sécurité : le nœud réinséré n'a pas de successeur pour l'instant
    pool->nodes[free_idx].next_free_idx = -1;

    if (pool->tail_idx == -1) {
        // Si la FIFO était complètement vide, la tête et la queue pointent sur ce nœud
        pool->head_idx = free_idx;
        pool->tail_idx = free_idx;
    } else {
        // Sinon, on accroche le nouveau nœud à la suite de l'ancienne queue
        pool->nodes[pool->tail_idx].next_free_idx = free_idx;
        // Et le nouveau nœud devient la nouvelle queue
        pool->tail_idx = free_idx;
    }

    pool->current_count++;

    // On réveille le Reader s'il attendait une page libre
    pthread_cond_signal(&pool->cond_free);

    pthread_mutex_unlock(&pool->lock);
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


void chronos_run(SharedMemoryPoolHeader *pool, int server_fd, struct sockaddr_in address, int addrlen, mqd_t atropos_mq) {
    int new_socket;
    int rc;
    printf("[⏳ CHRONOS] Initialisation des connexions et des moteurs persistants...\n");

    char path_world[1024];
    char path_river[1024];
    char path_delta[1024];

    // 1. Reconstruction des chemins sur le Ramdisk
    snprintf(path_world, sizeof(path_world), "%s/%s", ZYN_RAMDISK_PATH, ZYN_DB_WORLD);
    snprintf(path_river, sizeof(path_river), "%s/%s", ZYN_RAMDISK_PATH, ZYN_DB_RIVER);
    snprintf(path_delta, sizeof(path_delta), "%s/%s", ZYN_RAMDISK_PATH, ZYN_DB_DELTA);

    // 2. Ouverture de la base maîtresse (Relief)
    rc = sqlite3_open_v2(path_world, &g_db_main, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[❌ CHRONOS] Erreur ouverture base principale : %s\n", sqlite3_errmsg(g_db_main));
        return;
    }
    if (rc == SQLITE_OK) {
        // En cas de conflit d'accès sur le Ramdisk, SQLite attendra jusqu'à 1000ms 
        // que le concurrent relâche le verrou avant de renvoyer une erreur. 🛡️
        sqlite3_busy_timeout(g_db_main, 1000); 
    }
    fprintf(stdout, "[⏳ CHRONOS] Base maîtresse ouverte avec succès.\n");

    // 3. Unification de l'espace de nommage via ATTACH DATABASE
    char attach_query[2048];
    snprintf(attach_query, sizeof(attach_query), "ATTACH DATABASE '%s' AS rivers;", path_river);
    rc = sqlite3_exec(g_db_main, attach_query, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[❌ CHRONOS] Échec de l'ATTACH de la base rivières : %s\n", sqlite3_errmsg(g_db_main));
        sqlite3_close(g_db_main);
        return;
    }
    fprintf(stdout, "[🔗 CHRONOS] Base rivières rattachée avec succès.\n");

    /* snprintf(attach_query, sizeof(attach_query), "ATTACH DATABASE '%s' AS deltas;", path_delta);
    rc = sqlite3_exec(g_db_main, attach_query, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[❌ CHRONOS] Échec de l'ATTACH de la base deltas : %s\n", sqlite3_errmsg(g_db_main));
        sqlite3_close(g_db_main);
        return;
    }
    fprintf(stdout, "[🔗 CHRONOS] Base deltas rattachée avec succès.\n"); 
    */

    // 4. Injection des PRAGMA de compétition pour la performance pure
    const char *pragmas = 
        "PRAGMA journal_mode = WAL;"
        "PRAGMA synchronous = NORMAL;"
        "PRAGMA temp_store = MEMORY;"
        "PRAGMA mmap_size = 30000000;" 
        "PRAGMA journal_size_limit = 5242880;";

    rc = sqlite3_exec(g_db_main, pragmas, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[⚠️ CHRONOS] Attention, certains PRAGMA ont été refusés : %s\n", sqlite3_errmsg(g_db_main));
    }
    fprintf(stdout, "[🟢 CHRONOS] SQLite3 configuré en mode haute performance sur le Ramdisk.\n");

    // 5. Préparation de la requête optimisée compilée une seule fois
    const char *sql = "SELECT data FROM macro_chunks WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db_main, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[❌ CHRONOS] Échec de préparation de la requête SQL\n");
        sqlite3_close(g_db_main);
        return;
    }

    // 6. Connexion à la file de messages de retour Atlas -> Chronos
    mqd_t recv_mq = mq_open(ZYN_CHRONOS_RECV_MQ_NAME, O_RDWR);
    if (recv_mq == (mqd_t)-1) {
        perror("[❌ CHRONOS] Impossible d'ouvrir la MQ de réception de retour");
        sqlite3_finalize(stmt);
        sqlite3_close(g_db_main);
        return;
    }
    fprintf(stdout, "[📥 CHRONOS] Tunnel MQ de retour connecté. Prêt pour le Full-Duplex.\n");

    // =========================================================================
    // BOUCLE PRINCIPALE D'ACCEPTATION (Gestion des connexions clients B3)
    // =========================================================================
    while (!g_shutdown_requested) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        
        if (new_socket < 0) {
            if (errno == EINTR) break; // Sortie propre si Ctrl+C ou signal d'arrêt
            perror("[❌ CHRONOS] Échec accept");
            continue;
        }

        printf("\n[✅ CHRONOS] Client B3 connecté ! Déploiement des turbines de session...\n");

        // Configuration de l'environnement partagé pour les threads de la session actuelle
        ChronosSessionArgs session_args;
        session_args.pool = pool;
        session_args.socket_fd = new_socket;
        session_args.atropos_mq = atropos_mq;
        session_args.recv_mq = recv_mq;
        session_args.stmt = stmt;

        pthread_t reader_tid, writer_tid;

        // Allumage simultané de la double turbine sur la même socket
        if (pthread_create(&reader_tid, NULL, chronos_reader_thread, &session_args) != 0 ||
            pthread_create(&writer_tid, NULL, chronos_writer_thread, &session_args) != 0) {
            fprintf(stderr, "[❌ CHRONOS] Échec critique lors du déploiement des threads de session.\n");
            close(new_socket);
            continue;
        }

        // Le thread principal se cale ici et attend la fin de vie du Reader (déconnexion du client)
        pthread_join(reader_tid, NULL);
        
        // Par sécurité, on ferme le descripteur de fichier réseau pour forcer 
        // le Writer (bloqué sur son mq_receive ou en cours) à briser sa boucle.
        close(new_socket);
        pthread_join(writer_tid, NULL);

        printf("[✅ CHRONOS] Session terminée proprement avec le client B3. Turbines démontées.\n");
        printf("[⏳ CHRONOS] Retour en attente d'un nouveau client...\n");
    }

    // =========================================================================
    // NETTOYAGE FINAL DE LA COUCHE CHRONOS
    // =========================================================================
    printf("[🛑 CHRONOS] Fermeture des moteurs et libération des descripteurs...\n");
    mq_close(recv_mq);
    sqlite3_finalize(stmt);
    sqlite3_close(g_db_main);
    printf("[👋 CHRONOS] Extinction complète validée.\n");
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
    signal(SIGPIPE, SIG_IGN);
    
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
        
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
    if ((server_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0)) == 0) {
        perror("[❌ CHRONOS] Échec de la création de la socket");
        exit(EXIT_FAILURE);
    }

    // 2. Configuration des options de la socket (Réutilisation rapide du port)
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("[❌ CHRONOS] Échec de setsockopt (SO_REUSEADDR)");
        close(server_fd);
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
 
