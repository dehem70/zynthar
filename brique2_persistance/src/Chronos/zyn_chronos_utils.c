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
#include <sqlite3.h>
#include "zyn_chronos_utils.h"
#include "zyn_chronos.h"

// Connexion d'autorité unique gérée par Chronos
static sqlite3 *g_db_main = NULL;
static volatile sig_atomic_t g_shutdown_requested = 0;

void handle_shutdown_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        g_shutdown_requested = 1;
    }
}

// Fonction spécialisée dans la requête SQLite et l'injection des données dans la page SHM
void chronos_fetch_and_populate_context(sqlite3_stmt *stmt, SharedMemoryPoolHeader *pool, int32_t idx, int64_t macro_chunk_id) {
    sqlite3_bind_int64(stmt, 1, macro_chunk_id); 
    
    int success = 0; // Petit flag local pour suivre la réussite de l'extraction

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const void *blob_ptr = sqlite3_column_blob(stmt, 0);
        int blob_size = sqlite3_column_bytes(stmt, 0);
        
        if (blob_ptr && blob_size >= (int)sizeof(MacroChunk_db)) {
            const MacroChunk_db *macro_data = (const MacroChunk_db *)blob_ptr;
            
            // 🎯 Cas Nominal : Tout est correct, on peuple le contexte
            pool->nodes[idx].context.biome = macro_data->biome;
            pool->nodes[idx].context.coin_nw = macro_data->elevation_coin_nw;
            pool->nodes[idx].context.coin_ne = macro_data->elevation_coin_ne;
            pool->nodes[idx].context.coin_sw = macro_data->elevation_coin_sw;
            pool->nodes[idx].context.coin_se = macro_data->elevation_coin_se;
            pool->nodes[idx].context.temperature_raw = macro_data->temperature_raw;
            
            success = 1; // On valide le succès !
        } else {
            fprintf(stderr, "[⚠️ READER] BLOB invalide ou corrompu pour macro_id %ld\n", (long)macro_chunk_id);
        }
    }

    // 🎯 Si l'extraction a échoué (introuvable, vide ou corrompu), on applique les valeurs par défaut
    if (!success) {
        pool->nodes[idx].context.biome = 0;
        pool->nodes[idx].context.coin_nw = 0;
        pool->nodes[idx].context.coin_ne = 0;
        pool->nodes[idx].context.coin_sw = 0;
        pool->nodes[idx].context.coin_se = 0;
        pool->nodes[idx].context.temperature_raw = 0;
    }

    // 🎯 Clôture systématique et centralisée du statement (Plus de duplication)
    sqlite3_reset(stmt);
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
            // A. Traitement direct (Zéro-Copy)
            ChunkRequestPacket *packet = (ChunkRequestPacket *)buffer;
            int64_t macro_chunk_id = ntohl(packet->macro_chunk_id);

            printf("[✅ READER] REQUÊTE : id %ld | Micro[%u, %u, %u] | LOD %u\n", 
                   (long)macro_chunk_id, packet->mc_x, packet->mc_y, packet->mc_z, packet->lod);

            // B. Pioche d'un contexte libre dans la SHM de Cerbère (Bloquant si vide)
            int32_t idx = chronos_pop_shm_context(args->pool);

            // C. Mappage ou récupération du pointeur physique de 16 Mo via le cache local
            uint8_t *voxels_page = chronos_get_and_map_page(args->pool, idx);
            if (!voxels_page) {
                continue; // Sécurité mmap
            }

            // Remplissage des données géométriques de la requête
            args->pool->nodes[idx].context.lod  = packet->lod;
            args->pool->nodes[idx].context.mc_x = packet->mc_x;
            args->pool->nodes[idx].context.mc_y = packet->mc_y;
            args->pool->nodes[idx].context.mc_z = packet->mc_z;

            // D. Extraction Éclair depuis SQLite3 via la fonction dédiée
            chronos_fetch_and_populate_context(args->stmt, args->pool, idx, macro_chunk_id);

            // E. Historisation des deltas (Emplacement préservé)
            // voxels_page[index] = ... 

            // F. Préparation des métadonnées du contexte
            args->pool->nodes[idx].context.macro_id = macro_chunk_id;
            args->pool->nodes[idx].context.context_id = idx;
            args->pool->nodes[idx].context.jobs_remaining = 4096;
            
            // Passage du flambeau atomique
            __atomic_store_n(&args->pool->nodes[idx].context.status, ZYN_STATUS_COMPUTING, __ATOMIC_RELEASE);

            // G. Notification à Atropos via la fonction dédiée
            chronos_notify_atropos(args->atropos_mq, idx);

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
// Fonction isolée gérant la cinématique de notification résiliente vers Atropos
void chronos_notify_atropos(mqd_t atropos_mq, int32_t idx) {
    AtroposMessage msg;
    msg.shm_node_idx = idx;

    if (mq_send(atropos_mq, (const char *)&msg, sizeof(AtroposMessage), 0) == -1) {
        int retries = 0;
        while (mq_send(atropos_mq, (const char *)&msg, sizeof(AtroposMessage), 0) == -1 && retries < 5) {
            usleep(1000); // Attente d'1 milliseconde avant retry
            retries++;
        }

        if (retries >= 5) {
            fprintf(stderr, "[❌ CRITIQUE] Reader n'a pas pu notifier Atropos pour la page %d après 5 essais !\n", idx);
        }
    } else {
        printf("[📥 READER] Page %d chargée en RAM. Signal envoyé à Atropos.\n", idx);
    }
}



// Fonction spécialisée dans l'expédition Non-Bloquante avec gestion de la Backpressure TCP
int chronos_send_payload_nonblocking(int socket_fd, const uint8_t *voxels_page, uint32_t size, uint64_t macro_id) {
    ssize_t total_sent = 0;
    int retries = 0;

    while (total_sent < size && !g_shutdown_requested) {
        // MSG_DONTWAIT force Linux à renvoyer EAGAIN au lieu de figer le thread
        ssize_t sent_bytes = send(socket_fd, voxels_page + total_sent, size - total_sent, MSG_DONTWAIT);

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
                    break; // On laisse expirer pour éviter le deadlock total
                }
                usleep(10); // Micro-respiration de 10 microsecondes pour laisser le noyau Linux souffler
            } else {
                perror("[❌ WRITER] Erreur fatale send");
                return -1; // Erreur socket fatale
            }
        } else {
            return -1; // Socket close proprement à l'autre bout
        }
    }

    if (total_sent >= size) {
        return 0; // Succès de l'envoi total
    }
    return 0; // Sortie par abandon de timeout (non-fatal pour le thread)
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
        
        if (node_idx_received == -1) {
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

            // 🚀 Appel de la couche d'envoi non-bloquante extraite à l'éponge
            int res = chronos_send_payload_nonblocking(args->socket_fd, voxels_page, size, macro_id);

            if (res == 0 && !g_shutdown_requested) {
                printf("[♻️ WRITER] BLOB envoyé avec succès pour Macro_ID %lu (%d octets) | Page SHM %d.\n", 
                       macro_id, size, node_idx_received);
            }

            // 🔓 LIBÉRATION DU SLOT MEMOIRE (Quoi qu'il arrive, pour éviter les fuites de pages)
            __atomic_store_n(&args->pool->nodes[node_idx_received].context.status, ZYN_STATUS_FREE, __ATOMIC_RELEASE);
            chronos_push_shm_context(args->pool, node_idx_received);

            // Si l'envoi a retourné une erreur fatale (-1), on démonte la turbine
            if (res < 0) {
                break;
            }
        }
    }

    return NULL;
}

/**
 * @brief Pioche un nœud libre dans le pool partagé de manière bloquante / adaptative.
 * @return L'index du nœud pioché (0 à MAX_POOL_PAGES-1).
 */
int32_t chronos_pop_shm_context(SharedMemoryPoolHeader *pool) {
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
void chronos_push_shm_context(SharedMemoryPoolHeader *pool, int32_t free_idx) {
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
uint8_t* chronos_get_and_map_page(SharedMemoryPoolHeader *pool, int32_t idx) {
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
// A. Vérification et création du dossier de stockage physique
void chronos_ensure_storage_tree(void) {
    struct stat st = {0};
    if (stat("world", &st) == -1) {
        if (mkdir("world", 0777) == -1) {
            perror("[❌ CHRONOS] Impossible de créer le répertoire d'autorité 'world'");
            exit(EXIT_FAILURE);
        }
    }
}
// A. Centralisation exclusive de l'assemblage des chemins d'accès Ramdisk
void chronos_prepare_db_paths(char *path_world, char *path_river, char *path_delta, size_t max_len) {
    snprintf(path_world, max_len, "%s/%s", ZYN_RAMDISK_PATH, ZYN_DB_WORLD);
    snprintf(path_river, max_len, "%s/%s", ZYN_RAMDISK_PATH, ZYN_DB_RIVER);
    snprintf(path_delta, max_len, "%s/%s", ZYN_RAMDISK_PATH, ZYN_DB_DELTA);
}


// A. Boucle dynamique d'ouverture et de raccordement (ATTACH) des bases
void chronos_setup_sqlite_engine(const ChronosDatabaseTarget *targets, size_t target_count) {
    int rc;

    for (size_t i = 0; i < target_count; i++) {
        if (targets[i].is_main) {
            // Ouverture de la base maîtresse d'autorité (Relief)
            rc = sqlite3_open_v2(targets[i].path, &g_db_main, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "[❌ CHRONOS] Erreur ouverture base principale : %s\n", sqlite3_errmsg(g_db_main));
                exit(EXIT_FAILURE);
            }
            sqlite3_busy_timeout(g_db_main, 1000); 
            fprintf(stdout, "[⏳ CHRONOS] Base maîtresse principale ouverte avec succès.\n");
        } else {
            // Raccordement dynamique via ATTACH pour les bases secondaires (rivers, deltas, etc.)
            char attach_query[2048];
            snprintf(attach_query, sizeof(attach_query), "ATTACH DATABASE '%s' AS %s;", targets[i].path, targets[i].alias);
            
            rc = sqlite3_exec(g_db_main, attach_query, NULL, NULL, NULL);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "[❌ CHRONOS] Échec de l'ATTACH de la base '%s' (%s) : %s\n", 
                        targets[i].alias, targets[i].path, sqlite3_errmsg(g_db_main));
                sqlite3_close(g_db_main);
                exit(EXIT_FAILURE);
            }
            fprintf(stdout, "[🔗 CHRONOS] Base '%s' rattachée avec succès.\n", targets[i].alias);
        }
    }
}

// B. Injection isolée des PRAGMAs de compétition pour la performance pure
void chronos_inject_performance_pragmas(void) {
    const char *pragmas = 
        "PRAGMA journal_mode = WAL;"
        "PRAGMA synchronous = NORMAL;"
        "PRAGMA temp_store = MEMORY;"
        "PRAGMA mmap_size = 30000000;" 
        "PRAGMA journal_size_limit = 5242880;";

    int rc = sqlite3_exec(g_db_main, pragmas, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[⚠️ CHRONOS] Attention, certains PRAGMA ont été refusés : %s\n", sqlite3_errmsg(g_db_main));
    }
    fprintf(stdout, "[🟢 CHRONOS] SQLite3 configuré en mode haute performance sur le Ramdisk.\n");
}

// C. Compilation de la requête d'autorité et raccordement au tunnel Message Queue
sqlite3_stmt* chronos_bind_ipc_tunnels(mqd_t *recv_mq) {
    const char *sql = "SELECT data FROM macro_chunks WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    
    if (sqlite3_prepare_v2(g_db_main, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[❌ CHRONOS] Échec de préparation de la requête SQL\n");
        sqlite3_close(g_db_main);
        exit(EXIT_FAILURE);
    }

    *recv_mq = mq_open(ZYN_CHRONOS_RECV_MQ_NAME, O_RDWR);
    if (*recv_mq == (mqd_t)-1) {
        perror("[❌ CHRONOS] Impossible d'ouvrir la MQ de réception de retour");
        sqlite3_finalize(stmt);
        sqlite3_close(g_db_main);
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "[📥 CHRONOS] Tunnel MQ de retour connecté. Prêt pour le Full-Duplex.\n");

    return stmt;
}



sqlite3_stmt* chronos_init_storage_and_tunnels(mqd_t *recv_mq) {
    char path_world[1024];
    char path_river[1024];
    char path_delta[1024];

    // 1. Génération des chemins d'accès Ramdisk
    chronos_prepare_db_paths(path_world, path_river, path_delta, sizeof(path_world));

    // 2. Déclaration élégante du tableau des bases cibles
    ChronosDatabaseTarget targets[] = {
        { .path = path_world, .alias = "main",   .is_main = 1 },
        { .path = path_river, .alias = "rivers", .is_main = 0 }
        // 💡 Pour ajouter le Delta plus tard, il suffira de décommenter la ligne suivante :
        // { .path = path_delta, .alias = "deltas", .is_main = 0 }
    };
    size_t target_count = sizeof(targets) / sizeof(targets[0]);

    // 3. Ouverture et raccordements en boucle
    chronos_setup_sqlite_engine(targets, target_count);

    // 4. Application de la stratégie de performance
    chronos_inject_performance_pragmas();

    // 5. Raccordement final des tunnels IPC
    return chronos_bind_ipc_tunnels(recv_mq);
}

 
 // A. Validation des arguments de la ligne de commande
void chronos_parse_args(int argc, char *argv[], char **shm_name) {
    if (argc < 2) {
        fprintf(stderr, "[❌ CHRONOS] Erreur critique : Nom du segment SHM manquant.\n");
        fprintf(stderr, "Utilisation : %s <shm_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    *shm_name = argv[1];
}

// B. Projection du segment de mémoire partagée maître
SharedMemoryPoolHeader* chronos_attach_shm(const char *shm_name) {
    int ctrl_fd = shm_open(shm_name, O_RDWR, 0666);
    if (ctrl_fd == -1) {
        perror("[❌ CHRONOS] Erreur lors de l'ouverture du segment SHM maître");
        exit(EXIT_FAILURE);
    }

    SharedMemoryPoolHeader *pool = (SharedMemoryPoolHeader*)mmap(
        NULL, sizeof(SharedMemoryPoolHeader), PROT_READ | PROT_WRITE, MAP_SHARED, ctrl_fd, 0
    );
    close(ctrl_fd); // Le descripteur n'est plus requis après le mmap

    if (pool == MAP_FAILED) {
        perror("[❌ CHRONOS] Échec critique du mmap de la SHM maîtresse");
        exit(EXIT_FAILURE);
    }
    return pool;
}

// C. Création et configuration de la Socket Serveur TCP
int chronos_create_server_socket(struct sockaddr_in *address, int *addrlen) {
    int server_fd;
    int opt = 1;

    // Création de la socket avec SOCK_CLOEXEC pour éviter les fuites dans les sous-processus
    if ((server_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0)) == 0) {
        perror("[❌ CHRONOS] Échec de la création de la socket");
        exit(EXIT_FAILURE);
    }

    // Configuration des options de la socket (Réutilisation rapide du port)
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("[❌ CHRONOS] Échec de setsockopt (SO_REUSEADDR)");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY; 
    address->sin_port = htons(ZYN_CHRONOS_PORT);
    *addrlen = sizeof(*address);

    // Liaison de la socket au port 6969
    if (bind(server_fd, (struct sockaddr *)address, sizeof(*address)) < 0) {
        perror("[❌ CHRONOS] Échec du bind sur le port");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Passage en mode écoute
    if (listen(server_fd, 10) < 0) {
        perror("[❌ CHRONOS] Échec du listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("[+] Chronos est en ligne et écoute sur le port %d\n\n", ZYN_CHRONOS_PORT);
    return server_fd;
}

void chronos_run(SharedMemoryPoolHeader *pool, int server_fd, struct sockaddr_in address, int addrlen, mqd_t atropos_mq) {
    int new_socket;
    mqd_t recv_mq = (mqd_t)-1;

    printf("[⏳ CHRONOS] Initialisation des connexions et des moteurs persistants...\n");

    // 1. Appel de l'initialisation extraite à l'éponge
    sqlite3_stmt *stmt = chronos_init_storage_and_tunnels(&recv_mq);

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

        // Configuration de l'environnement partagé via le type NATIF de ton projet 🎯
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

        // Le thread principal attend la fin de vie du Reader (déconnexion du client)
        pthread_join(reader_tid, NULL);
        
        // Fermeture forcée de la socket pour réveiller le Writer
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


