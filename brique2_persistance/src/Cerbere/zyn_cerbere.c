/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 07/06/2026
 *
 * zyn_cerbere  :
 * utilisation :
 *
 * =============================================================================*/
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mqueue.h>
#include <sys/eventfd.h>

// Inclusions des référentiels du projet
#include <zynthar.h>
#include "zyn_cerbere.h"
#include "zyn_b2_memory_pool.h"

static WatchdogArgs watchdog_context = {0};
static pthread_t backup_thread_id;
static volatile sig_atomic_t g_cerbere_running = 1;
static pid_t g_chronos_pid = -1;


/**
 * @brief Gestionnaire de signaux système pour interception d'arrêt (SIGINT, SIGTERM).
 */
// Handler pour que Cerbère intercepte le Ctrl+C et le propage à Chronos
void handle_cerbere_shutdown(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        g_cerbere_running = 0;
        if (g_chronos_pid > 0) {
            // On relaie proprement le signal d'arrêt à Chronos pour qu'il flush et ferme ses sockets
            kill(g_chronos_pid, sig);
        }
    }
}

void cerbere_init_shm_pool(SharedMemoryPoolHeader *pool, int32_t initial_size) {
    // 1. Initialisation des attributs de synchronisation INTER-PROCESSUS (crucial pour execv)
    pthread_mutexattr_t mutex_attr;
    pthread_condattr_t cond_attr;
    
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&pool->lock, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);
    
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&pool->cond_free, &cond_attr);
    pthread_condattr_destroy(&cond_attr);
    
    // 2. Configuration des variables d'état
    pool->head_idx = -1; // Initialisé à -1, sera configuré lors de l'allocation des pages
    pool->tail_idx = -1;
    pool->current_count = 0;
    pool->low_watermark = initial_size / 2;
    
// Initialisation du tableau fixe à blanc
    for (int32_t i = 0; i < MAX_POOL_PAGES; i++) {
        pool->nodes[i].is_allocated = 0;
        pool->nodes[i].next_free_idx = -1;
    }
    // 1. Initialisation des pointeurs globaux de la queue
pool->atropos_queue.head = 0;
pool->atropos_queue.tail = 0;

// 🎯 RAJOUT DE LA BOUCLE DE SÉQUENÇAGE INITIAL (Vyukov Ring Buffer)
// Chaque slot doit posséder sa propre identité de cycle au démarrage (ticket_id = i)
// pour que le premier passage d'Atropos (qui cherche ticket_id == current_tail) valide immédiatement le slot.
for (uint64_t i = 0; i < NANO_QUEUE_SIZE; i++) {
    pool->atropos_queue.buffer[i].ticket_id = i;
}
    

    // 3. 🚀 ÉTAPE 4 : Pré-allocation et chaînage de départ en mode FIFO
    int32_t last_allocated_idx = -1;

    for (int32_t i = 0; i < initial_size && i < MAX_POOL_PAGES; i++) {
        if (cerbere_allocate_shm_page(pool, i) == 0) {
            pool->nodes[i].next_free_idx = -1; // C'est le nouveau bout de la file

            if (pool->head_idx == -1) {
                // C'est la toute première page allouée : elle devient la tête
                pool->head_idx = i;
            } else {
                // On rattache la page précédente à celle-ci
                pool->nodes[last_allocated_idx].next_free_idx = i;
            }
            
            last_allocated_idx = i;
            pool->tail_idx = i; // La queue se déplace à chaque ajout
            pool->current_count++;
            
            // Initialisation sémantique applicative pour Atropos/Atlas/Forgerons
            pool->nodes[i].context.status = 255; // ZYN_STATUS_FREE
        }
    }
    printf("[🐕 CERBÈRE] FIFO initiale chaînée : Tête -> Page %d | Queue -> Page %d\n", pool->head_idx, pool->tail_idx);
}
void* cerbere_shm_monitoring_thread(void *arg) {
    SharedMemoryPoolHeader *pool = (SharedMemoryPoolHeader*)arg;
    uint32_t loop_counter = 0;

    while (1) {
/*        usleep(10000); // 10ms
        loop_counter++;

        pthread_mutex_lock(&pool->lock);
        int32_t current = pool->current_count;
        int32_t watermark = pool->low_watermark;
        pthread_mutex_unlock(&pool->lock);

        if (current <= watermark) {
            pthread_mutex_lock(&pool->lock);
            
            int32_t allocated_this_turn = 0;
            for (int32_t i = 0; i < MAX_POOL_PAGES && allocated_this_turn < 2; i++) {
                if (!pool->nodes[i].is_allocated) {
                    
                    pthread_mutex_unlock(&pool->lock);
                    int rc = cerbere_allocate_shm_page(pool, i);
                    pthread_mutex_lock(&pool->lock);
                    
                    if (rc == 0) {
                        // 🎯 Insertion étanche en QUEUE de la FIFO
                        pool->nodes[i].next_free_idx = -1;
                        pool->nodes[i].context.status = 255; // ZYN_STATUS_FREE

                        if (pool->tail_idx == -1) {
                            pool->head_idx = i;
                            pool->tail_idx = i;
                        } else {
                            pool->nodes[pool->tail_idx].next_free_idx = i;
                            pool->tail_idx = i;
                        }

                        pool->current_count++;
                        allocated_this_turn++;
                        
                        printf("[🐕 CERBÈRE] Backpressure : Injection dynamique de la Page %d en queue de FIFO.\n", i);
                        pthread_cond_signal(&pool->cond_free); // Réveille Chronos s'il attendait
                    }
                }
            }
            pthread_mutex_unlock(&pool->lock);
        }

        // 🐕 LISSAGE PASSIF : Désactivé ou mis en commentaire pour la V0.1 
        // Le parcours actuel détruirait le chaînage FIFO complexe en cas de charge.
        // On le réécrira proprement en mode FIFO lors de la V0.2 !
        /*if (loop_counter >= 30000) {
            loop_counter = 0;
            pthread_mutex_lock(&pool->lock);
            
            // Si on a trop de stock inutilisé, on libère le dernier élément de la LIFO
            if (pool->low_watermark > 4 && pool->current_count > (pool->low_watermark * 2)) {
                int32_t prev_idx = -1;
                int32_t curr_idx = pool->top_idx;
                
                // Parcours pour trouver le dernier nœud de la pile
                while (curr_idx != -1 && pool->nodes[curr_idx].next_free_idx != -1) {
                    prev_idx = curr_idx;
                    curr_idx = pool->nodes[curr_idx].next_free_idx;
                }
                
                if (curr_idx != -1) {
                    // Retrait du nœud du chaînage
                    if (prev_idx != -1) {
                        pool->nodes[prev_idx].next_free_idx = -1;
                    } else {
                        pool->top_idx = -1;
                    }
                    
                    // Destruction physique de la page SHM (Rend la RAM à la machine)
                    shm_unlink(pool->nodes[curr_idx].context.shm_page_name);
                    pool->nodes[curr_idx].is_allocated = 0;
                    pool->nodes[curr_idx].next_free_idx = -1;
                    pool->current_count--;
                    
                    if (pool->low_watermark > 4) pool->low_watermark--;
                }
            }
            pthread_mutex_unlock(&pool->lock);
        }*/
        sleep(1);
    }
    return NULL;
}
/**
 * @brief Parseur de configuration linéaire (zéro allocation dynamique).
 */
static int parse_config(CerbereConfig *config) {
    char *root_env = getenv(ZYN_CONFIG_PATH_ENV);
    if (!root_env) {
        fprintf(stderr, "[❌ CERBÈRE] Erreur : La variable d'environnement %s n'est pas définie.\n", ZYN_CONFIG_PATH_ENV);
        return -1;
    }
    strncpy(config->root_path, root_env, sizeof(config->root_path) - 1);

    char config_file_path[1024];
    snprintf(config_file_path, sizeof(config_file_path), "%s/config/zynthar_config.json", config->root_path);

    FILE *f = fopen(config_file_path, "r");
    if (!f) {
        fprintf(stderr, "[⚠️ CERBÈRE] zynthar_config.json introuvable. Repli sur la valeur par défaut.\n");
        config->backup_interval_seconds = ZYN_DEFAULT_BACKUP_INTERVAL_SEC;
        return 0;
    }

    char buffer[2048];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[bytes_read] = '\0';
    fclose(f);

    char *key = strstr(buffer, "\"backup_interval_seconds\"");
    if (key) {
        char *colon = strchr(key, ':');
        if (colon) {
            config->backup_interval_seconds = (uint32_t)strtoul(colon + 1, NULL, 10);
        }
    } else {
        config->backup_interval_seconds = ZYN_DEFAULT_BACKUP_INTERVAL_SEC;
    }

    return 0;
}

/**
 * @brief Copie binaire par blocs alignés (64 Ko) pour saturer le bus I/O.
 */
static int copy_file(const char *src, const char *dst) {
    FILE *source = fopen(src, "rb");
    if (!source) return -1;

    FILE *dest = fopen(dst, "wb");
    if (!dest) {
        fclose(source);
        return -1;
    }

    uint8_t buffer[65536]; 
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        fwrite(buffer, 1, bytes, dest);
    }

    fclose(source);
    fclose(dest);
    return 0;
}

/**
 * @brief Routine asynchrone du thread de garde (Flush disk périodique).
 */
static void* backup_worker(void *arg) {
    WatchdogArgs *args = (WatchdogArgs*)arg;
    fprintf(stdout, "[🔒 CERBÈRE] Thread de flush actif (Intervalle : %ds).\n", args->interval_sec);

    while (args->keep_running) {
        for (uint32_t i = 0; i < args->interval_sec && args->keep_running; i++) {
            sleep(1);
        }

        if (!args->keep_running) break;

        fprintf(stdout, "[💾 CERBÈRE] Synchronisation de %s vers stockage persistant...\n", ZYN_DB_DELTA);
        if (copy_file(args->src_deltas, args->dst_deltas) != 0) {
            fprintf(stderr, "[❌ CERBÈRE] ÉCHEC CRITIQUE de la sauvegarde du Delta !\n");
        } else {
            fprintf(stdout, "[✅ CERBÈRE] Flush du Delta validé.\n");
        }
    }
    return NULL;
}

int cerbere_init(void) {
    CerbereConfig config = {0};
    if (parse_config(&config) != 0) return -1;

    // 1. Montage du point d'accès Ramdisk tmpfs
    struct stat st = {0};
    if (stat(ZYN_RAMDISK_PATH, &st) == -1) {
        if (mkdir(ZYN_RAMDISK_PATH, 0777) != 0) {
            fprintf(stderr, "[❌ CERBÈRE] Impossible de créer le point de staging : %s\n", strerror(errno));
            return -1;
        }
    }

    char mount_options[64];
    snprintf(mount_options, sizeof(mount_options), "size=%d", ZYN_RAMDISK_SIZE_BYTES);
    if (mount("tmpfs", ZYN_RAMDISK_PATH, "tmpfs", 0, mount_options) != 0) {
        if (errno != EBUSY) {
            fprintf(stderr, "[❌ CERBÈRE] Échec du montage tmpfs : %s\n", strerror(errno));
            return -1;
        }
    }
    fprintf(stdout, "[🚀 CERBÈRE] Ramdisk tmpfs initialisé sur %s (%d Mo).\n", ZYN_RAMDISK_PATH, ZYN_RAMDISK_SIZE_BYTES / (1024 * 1024));

    // 2. Configuration des chemins absolus via les métadonnées de zynthar.h
    char src_relief[1024], dst_relief[1024];
    char src_rivers[1024], dst_rivers[1024];
    char src_deltas[1024], dst_deltas[1024];

    snprintf(src_relief, sizeof(src_relief), "%s/%s%s", config.root_path, ZYN_DB_EMPLACEMENT, ZYN_DB_WORLD);
    snprintf(dst_relief, sizeof(dst_relief), "%s/%s", ZYN_RAMDISK_PATH, ZYN_DB_WORLD);
    
    snprintf(src_rivers, sizeof(src_rivers), "%s/%s%s", config.root_path, ZYN_DB_EMPLACEMENT, ZYN_DB_RIVER);
    snprintf(dst_rivers, sizeof(dst_rivers), "%s/%s", ZYN_RAMDISK_PATH, ZYN_DB_RIVER);
    
    snprintf(src_deltas, sizeof(src_deltas), "%s/%s%s", config.root_path, ZYN_DB_EMPLACEMENT, ZYN_DB_DELTA);
    snprintf(dst_deltas, sizeof(dst_deltas), "%s/%s", ZYN_RAMDISK_PATH, ZYN_DB_DELTA);

    // 3. Staging / Chargement initial en RAM au Bootstrapping
    fprintf(stdout, "[🚚 CERBÈRE] Chargement des bases de données SQLite3 en RAM...\n");
    if (copy_file(src_relief, dst_relief) != 0) {
        fprintf(stderr, "[⚠️ CERBÈRE] %s manquant sur stockage persistant.\n", ZYN_DB_WORLD);
    }
    if (copy_file(src_rivers, dst_rivers) != 0) {
        fprintf(stderr, "[⚠️ CERBÈRE] %s manquant sur stockage persistant.\n", ZYN_DB_RIVER);
    }
    
    // Reprise à chaud : charge l'état des modifications s'il existe
    copy_file(src_deltas, dst_deltas);

    // 4. Spawn du Worker Asynchrone
    strncpy(watchdog_context.src_deltas, dst_deltas, sizeof(watchdog_context.src_deltas) - 1);
    strncpy(watchdog_context.dst_deltas, src_deltas, sizeof(watchdog_context.dst_deltas) - 1);
    watchdog_context.interval_sec = config.backup_interval_seconds;
    watchdog_context.keep_running = 1;

    if (pthread_create(&backup_thread_id, NULL, backup_worker, &watchdog_context) != 0) {
        fprintf(stderr, "[❌ CERBÈRE] Impossible de lancer le thread de sauvegarde.\n");
        return -1;
    }
    
    // --- INITIALISATION DE LA FILE DE MESSAGES ---
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = ZYN_MQ_MAX_MSG;
    attr.mq_msgsize = sizeof(AtroposMessage);
    attr.mq_curmsgs = 0;

    // On s'assure de nettoyer une vieille file fantôme au cas où
    mq_unlink(ZYN_ATROPOS_MQ_NAME);

    mqd_t mq = mq_open(ZYN_ATROPOS_MQ_NAME, O_CREAT | O_RDWR, 0666, &attr);
    if (mq == (mqd_t)-1) {
        perror("[❌ CERBÈRE] Échec de la création de la Message Queue");
        return -1;
    } else {
        printf("[🐕 CERBÈRE] Message Queue '%s' initialisée avec succès.\n", ZYN_ATROPOS_MQ_NAME);
        mq_close(mq); // On la ferme localement, les fils l'ouvriront eux-mêmes
    }


    // 🎯 CRÉATION DE LA MESSAGE QUEUE DE RETOUR POUR CHRONOS
    struct mq_attr recv_attr;
    recv_attr.mq_flags = 0;
    recv_attr.mq_maxmsg = 10;
    recv_attr.mq_msgsize = sizeof(int); // On envoie juste l'index du nœud (un entier)
    recv_attr.mq_curmsgs = 0;
    
        // On s'assure de nettoyer une vieille file fantôme au cas où
    mq_unlink(ZYN_CHRONOS_RECV_MQ_NAME);

    mqd_t chronos_recv_mq = mq_open(ZYN_CHRONOS_RECV_MQ_NAME, O_CREAT | O_RDWR, 0666, &recv_attr);
    if (chronos_recv_mq == (mqd_t)-1) {
        perror("[❌ CERBÈRE] Échec de la création de la MQ de retour de Chronos");
        return EXIT_FAILURE;
    } else {
        printf("[🐕 CERBÈRE] Message Queue '%s' initialisée avec succès.\n", ZYN_CHRONOS_RECV_MQ_NAME);
        mq_close(chronos_recv_mq); // On la ferme localement, les fils l'ouvriront eux-mêmes
    }
    
    
    
    
    return 0;
}

void cerbere_shutdown(void) {
    fprintf(stdout, "[🛑 CERBÈRE] Extinction demandée. Arrêt des threads...\n");
    watchdog_context.keep_running = 0;
    pthread_join(backup_thread_id, NULL);

    // 1. Reconstruction et nettoyage des fichiers volatils du Ramdisk
    char ram_world[1024];
    char ram_river[1024];
    char ram_delta[1024];
    snprintf(ram_world, sizeof(ram_world), "%s/%s", ZYN_RAMDISK_PATH, ZYN_DB_WORLD);
    snprintf(ram_river, sizeof(ram_river), "%s/%s", ZYN_RAMDISK_PATH, ZYN_DB_RIVER);
    snprintf(ram_delta, sizeof(ram_delta), "%s/%s", ZYN_RAMDISK_PATH, ZYN_DB_DELTA);

    fprintf(stdout, "[🧹 CERBÈRE] Suppression des fichiers du Ramdisk...\n");
    unlink(ram_world);
    unlink(ram_river);
    unlink(ram_delta);

    // 3. Démontage synchrone sur la cible
    fprintf(stdout, "[🔌 CERBÈRE] Libération synchrone du point de montage tmpfs...\n");
    if (umount(ZYN_RAMDISK_PATH) != 0) {
        // Si le umount classique échoue encore, on utilise le mode LAZY pour forcer le kernel à purger
        umount2(ZYN_RAMDISK_PATH, MNT_DETACH);
    }

    // Un micro-sommeil de sécurité pour laisser le VFS appliquer le détachement
    usleep(10000);

    // 4. Destruction physique du dossier
    if (rmdir(ZYN_RAMDISK_PATH) == 0) {
        fprintf(stdout, "[🗑️ CERBÈRE] Répertoire détruit proprement.\n");
    } else {
        // Si le code 16 persiste, il est confiné dans le dossier "trash", l'espace nominal est propre !
        fprintf(stdout, "[⚠️ CERBÈRE] Nettoyage en arrière-plan délégué au noyau Linux (Code: %d).\n", errno);
    }


    // À la toute fin du protocole de fermeture de Cerbère :
    mq_unlink(ZYN_ATROPOS_MQ_NAME);
    mq_unlink(ZYN_CHRONOS_RECV_MQ_NAME);
    printf("[🐕 CERBÈRE] Messages Queues purgées du système.\n");
    
    fprintf(stdout, "[💤 CERBÈRE] Subsystem offline. Atelier Zynthar prêt pour la prochaine session.\n");
}

// Configure physiquement un segment de 16 Mo pour un index donné
static int cerbere_allocate_shm_page(SharedMemoryPoolHeader *pool, int32_t idx) {
    char name[32];
    snprintf(name, sizeof(name), "/zynthar_page_%d", idx);
    
    // 1. Création du fichier de mémoire partagée POSIX
    int shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("[❌ CERBÈRE] Échec shm_open pour page dynamique");
        return -1;
    }
    
    // 2. Allocation physique de 16 Mo (Sparsité contrôlée par l'OS)
    if (ftruncate(shm_fd, PAGE_SIZE_16MO) == -1) {
        perror("[❌ CERBÈRE] Échec ftruncate 16 Mo");
        close(shm_fd);
        return -1;
    }
    
    // 3. Nettoyage initial et mise en cache CPU locale pour Cerbère
    uint8_t *page_ptr = (uint8_t*)mmap(NULL, PAGE_SIZE_16MO, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (page_ptr == MAP_FAILED) {
        perror("[❌ CERBÈRE] Échec mmap initialisation");
        close(shm_fd);
        return -1;
    }
    
    memset(page_ptr, 0, PAGE_SIZE_16MO); // Remplissage initial "Air" (0x00)
    munmap(page_ptr, PAGE_SIZE_16MO);
    close(shm_fd);
    
    // Enregistrement des métadonnées dans la table de contrôle
    strncpy(pool->nodes[idx].context.shm_page_name, name, sizeof(pool->nodes[idx].context.shm_page_name));
    pool->nodes[idx].context.context_id = (uint8_t)idx;
    pool->nodes[idx].is_allocated = 1;
    
    return 0;
}

void cerbere_cleanup_all_shm(SharedMemoryPoolHeader *pool) {
    if (!pool) return;

    printf("[🐕 CERBÈRE] Fermeture propre du serveur : Libération des pages mémoires...\n");
    
    pthread_mutex_lock(&pool->lock);
    
    // On balaie l'intégralité du tableau fixe des nœuds
    for (int32_t i = 0; i < MAX_POOL_PAGES; i++) {
        if (pool->nodes[i].is_allocated) {
            printf("[🐕 CERBÈRE] Libération de la page orpheline : %s\n", 
                    pool->nodes[i].context.shm_page_name);
            
            // Ordre destructif au noyau Linux : rends la RAM !
            shm_unlink(pool->nodes[i].context.shm_page_name);
            pool->nodes[i].is_allocated = 0;
        }
    }
    
    pthread_mutex_unlock(&pool->lock);
}
/**
 * @brief Point d'entrée principal du processus indépendant Cerbère.
 */
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    struct sigaction sa;
    sa.sa_handler = handle_cerbere_shutdown;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("=== ZYNTHAR v0.1 - ORCHESTRATEUR MAÎTRE : CERBÈRE ===\n");
    
    fprintf(stdout, "[⚙️ CERBÈRE] Démarrage du processus autonome de persistance...\n");

    // Initialisation globale du sous-système
    if (cerbere_init() != 0) {
        fprintf(stderr, "[❌ CERBÈRE] Échec critique lors de l'initialisation. Avortement.\n");
        return EXIT_FAILURE;
    }
    
    char *shm_control_name = "/zynthar_shm_pool";
    size_t shm_control_size = sizeof(SharedMemoryPoolHeader);
    
    // On s'assure d'un nettoyage d'une session précédente qui aurait crashé
    shm_unlink(shm_control_name); 

    int ctrl_fd = shm_open(shm_control_name, O_CREAT | O_RDWR, 0666);
    if (ctrl_fd == -1) {
        perror("[❌ CERBÈRE] Impossible de créer le segment de contrôle SHM");
        return EXIT_FAILURE;
    }
    if (ftruncate(ctrl_fd, shm_control_size) == -1) {
        perror("[❌ CERBÈRE] Erreur critique lors du ftruncate du segment maître");
        close(ctrl_fd);
        return EXIT_FAILURE;
    }

    SharedMemoryPoolHeader *global_pool = (SharedMemoryPoolHeader*)mmap(
        NULL, shm_control_size, PROT_READ | PROT_WRITE, MAP_SHARED, ctrl_fd, 0
    );
    close(ctrl_fd); // Le descripteur n'est plus requis après le mmap

    int32_t taille_initiale_pool = 64; 
    printf("[✅ CERBÈRE] Configuration SHM adaptative. Allocation initiale de %d pages...\n", taille_initiale_pool);
    
    cerbere_init_shm_pool(global_pool, taille_initiale_pool);
    printf("[✅ CERBÈRE] Pool de contrôle initialisé. Stock : %d\n", global_pool->current_count);

    // Lancement du thread avec le pointeur SHM
    pthread_t thread_cerbere;
    if (pthread_create(&thread_cerbere, NULL, cerbere_shm_monitoring_thread, global_pool) != 0) {
        perror("[❌ CERBÈRE] Impossible de lancer le thread de surveillance Cerbère");
        return EXIT_FAILURE;
    }
    pthread_detach(thread_cerbere); 
    printf("[✅ CERBÈRE] Thread de surveillance actif en arrière-plan.\n");

    // =========================================================================
    // INITIALISATION DE L'EVENTFD GLOBAL (FORGERONS ➔ ATLAS)
    // =========================================================================
    // EFD_NONBLOCK : Permet à Atlas de lire de manière non-bloquante si besoin
    // EFD_SEMAPHORE : Crucial ! Chaque lecture (read) par Atlas ne consommera 
    //                 qu'un seul événement (1), même si le compteur est à 10. 
    //                 Idéal pour traiter les pages une par une.
    int atlas_ev_fd = eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE);
    if (atlas_ev_fd == -1) {
        perror("[❌ CERBÈRE] Échec critique de la création de l'eventfd");
        // Nettoyage et sortie
        mq_unlink(ZYN_ATROPOS_MQ_NAME);
        shm_unlink(shm_control_name);
        return EXIT_FAILURE;
    }
    char ev_fd_str[16];
    sprintf(ev_fd_str, "%d", atlas_ev_fd);
    printf("[🐕 CERBÈRE] Canal événementiel EventFD initialisé (FD local : %d).\n", atlas_ev_fd);
    // ------------------------------------------------------------------
    // STEP 2 : LANCEMENT INDÉPENDANT DE CHRONOS (WORKER 0)
    // ------------------------------------------------------------------
    printf("[✅ CERBÈRE] Alignement des planètes... Lancement de Chronos (Worker 0).\n");
    
    g_chronos_pid = fork();
    
    pid_t g_forgerons_pids[NB_FORGERONS] = {0};

    if (g_chronos_pid < 0) {
        perror("[❌ CERBÈRE] Erreur critique lors du fork de Chronos");
        exit(EXIT_FAILURE);
    }

    if (g_chronos_pid == 0) {
       // --- PROCESSUS FILS : ÉXÉCUTION DE CHRONOS ---
       printf("[CERBERE] Redirection du processus fils vers zyn_chronos...\n");

       // On prépare l'argument : par exemple le nom du segment SHM ou l'ID
       // Ici, on imagine que le pool est partagé sous le nom "/zynthar_shm_pool"

       char *args[] = {"./brique2_persistance/zyn_chronos", shm_control_name, NULL};
    
       if (execv(args[0], args) == -1) {
           perror("[❌ CERBÈRE] Erreur critique : Impossible d'exécuter zyn_chronos");
           exit(EXIT_FAILURE);
       }
    }

    // --- PROCESSUS PÈRE : CERBÈRE EN SURVEILLANCE ---
    printf("[✅ CERBÈRE] Chronos démarré avec succès par Cerbère (PID: %d).\n", g_chronos_pid);
    
    // 2. 📐 LANCEMENT D'ATROPOS (La pièce manquante !)
    pid_t g_atropos_pid = fork();
    if (g_atropos_pid == 0) {
        // Nous sommes dans le fils Atropos
        // On lui passe exactement le même nom de segment SHM en argument (argv[1])
        char *atropos_args[] = {"./brique2_persistance/zyn_atropos", shm_control_name, NULL};
        execv(atropos_args[0], atropos_args);
        
        // Si execv revient, c'est une erreur critique
        perror("[❌ CERBÈRE] Échec du lancement d'Atropos via execv");
        exit(EXIT_FAILURE);
    } else if (g_atropos_pid < 0) {
        perror("[❌ CERBÈRE] Échec du fork pour Atropos");
        return EXIT_FAILURE;
    }
    printf("[🐕 CERBÈRE] Processus Atropos lancé avec le PID : %d\n", g_atropos_pid);
    
    // =========================================================================
    // 3. 🛠️ LANCEMENT DE L'ARMÉE DE FORGERONS
    // =========================================================================
    printf("[🐕 CERBÈRE] Déploiement de l'armée de %d Forgerons...\n", NB_FORGERONS);

    for (int i = 0; i < NB_FORGERONS; i++) {
        pid_t pid = fork();
        
        if (pid == 0) {
            char *forgeron_args[] = {"./brique2_persistance/zyn_forgeron", shm_control_name,ev_fd_str, NULL};
            execv(forgeron_args[0], forgeron_args);
            
            // Si execv revient, c'est un échec critique
            perror("[❌ CERBÈRE] Échec du lancement d'un Forgeron via execv");
            exit(EXIT_FAILURE);
        } else if (pid < 0) {
            perror("[❌ CERBÈRE] Échec du fork pour un Forgeron");
            return EXIT_FAILURE;
        }
        
        // Le père enregistre le PID du Forgeron
        g_forgerons_pids[i] = pid;
        printf("[🐕 CERBÈRE] Forgeron n°%d déployé avec le PID : %d\n", i + 1, pid);
    }
    // =========================================================================
    // 4. 🌍 LANCEMENT D'ATLAS
    // =========================================================================
    pid_t g_atlas_pid = fork();
    if (g_atlas_pid == 0) {
        // Nous sommes dans le processus fils d'Atlas
        char *atlas_args[] = {"./brique2_persistance/zyn_atlas", shm_control_name,ev_fd_str, NULL};
        execv(atlas_args[0], atlas_args);
        
        perror("[❌ CERBÈRE] Échec du lancement d'Atlas via execv");
        exit(EXIT_FAILURE);
    } else if (g_atlas_pid < 0) {
        perror("[❌ CERBÈRE] Échec du fork pour Atlas");
        return EXIT_FAILURE;
    }
    printf("[🐕 CERBÈRE] Processus Atlas déployé avec le PID : %d\n", g_atlas_pid);
    
    
    printf("[🛑 CERBÈRE] Cerbère passe en mode Watchdog passif. En attente de signaux...\n\n");

    // Boucle de surveillance de Cerbère
    while (g_cerbere_running) {
        int status;
        // waitpid non-bloquant (WNOHANG) pour vérifier si Chronos n'a pas crashé de lui-même
        pid_t result = waitpid(g_chronos_pid, &status, WNOHANG);
        
        if (result == g_chronos_pid) {
            // Chronos s'est arrêté prématurément
            if (WIFEXITED(status)) {
                printf("[❌ CERBÈRE] Alerte : Chronos s'est arrêté avec le code de sortie %d.\n", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("[❌ CERBÈRE] Alerte : Chronos a été tué par le signal %d.\n", WTERMSIG(status));
            }
            break; // Cerbère s'arrête si son worker principal meurt
        }
        result = waitpid(g_atropos_pid, &status, WNOHANG);
        
        if (result == g_atropos_pid) {
            // Atropos s'est arrêté prématurément
            if (WIFEXITED(status)) {
                printf("[❌ CERBÈRE] Alerte : Atropos s'est arrêté avec le code de sortie %d.\n", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("[❌ CERBÈRE] Alerte : Atropos a été tué par le signal %d.\n", WTERMSIG(status));
            }
            break; // Cerbère s'arrête si son worker principal meurt
        }
        result = waitpid(g_atlas_pid, &status, WNOHANG);
        if (result == g_atlas_pid) {
            // Atlas s'est arrêté prématurément
            if (WIFEXITED(status)) {
                printf("[❌ CERBÈRE] Alerte : Atlas s'est arrêté avec le code de sortie %d.\n", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("[❌ CERBÈRE] Alerte : Atlas a été tué par le signal %d.\n", WTERMSIG(status));
            }
            break; // Cerbère s'arrête si son worker principal meurt
        }


        // On dort 200ms entre chaque vérification de santé pour ne pas surcharger le CPU
        usleep(200000);
    }
    printf("\n[⚠️ CERBÈRE] Cerbère intercepte l'arrêt global du serveur.\n");
    if (g_chronos_pid > 0) {
        printf("[🛑 CERBÈRE] Attente de la confirmation d'extinction de Chronos...\n");
        waitpid(g_chronos_pid, NULL, 0); // Attente bloquante ici pour être sûr que Chronos a fini ses close()
        printf("[✅ CERBÈRE] Chronos a validé son nettoyage.\n");
    }
    printf("[🛑 CERBÈRE] Amorçage du protocole d'extinction global...\n");

    // 🚀 ENVOI DU JETON POISON À ATROPOS
    // On ouvre la file d'Atropos pour lui envoyer l'index 255 afin de le sortir de son mq_receive
    mqd_t atropos_mq = mq_open(ZYN_ATROPOS_MQ_NAME, O_WRONLY);
    if (atropos_mq != (mqd_t)-1) {
        AtroposMessage poison;
        poison.shm_node_idx = 255; // Notre sentinelle d'arrêt
        
        if (mq_send(atropos_mq, (const char*)&poison, sizeof(poison), 0) == -1) {
            perror("[❌ CERBÈRE] Échec de l'envoi du jeton poison à Atropos");
        } else {
            printf("[✅ CERBÈRE] Jeton d'évacuation envoyé à Atropos.\n");
        }
        mq_close(atropos_mq);
    }
     if (g_atropos_pid > 0) {
        printf("[🛑 CERBÈRE] Attente de la confirmation d'extinction d'Atropos...\n");
        waitpid(g_atropos_pid, NULL, 0); // Attente bloquante ici pour être sûr que Chronos a fini ses close()
        printf("[✅ CERBÈRE] Atropos a validé son nettoyage.\n");
    }   

    if (g_atlas_pid > 0) {
        printf("[🛑 CERBÈRE] Attente de la confirmation d'extinction d'Atlas...\n");
        // On envoie un SIGTERM civilisé à Atlas pour qu'il ferme son epoll proprement
        kill(g_atlas_pid, SIGTERM);
        waitpid(g_atlas_pid, NULL, 0);
        printf("[✅ CERBÈRE] Atlas a validé son nettoyage.\n");
    }

    // À la toute fin, juste avant le return 0 du main de Cerbère :
    close(atlas_ev_fd);

    // 3. 🛠️ Attente de l'armée des Forgerons
    printf("[🛑 CERBÈRE] Démobilisation des Forgerons...\n");
    for (int i = 0; i < NB_FORGERONS; i++) {
        if (g_forgerons_pids[i] > 0) {
            // Pour la V0.1, comme les Forgerons sont dans une boucle infinie de lecture du Ring Buffer,
            // on leur envoie un signal SIGTERM civilisé pour leur dire de s'arrêter proprement,
            // puis on attend qu'ils s'enterrent.
            kill(g_forgerons_pids[i], SIGTERM);
            waitpid(g_forgerons_pids[i], NULL, 0);
        }
    }
    printf("[✅ CERBÈRE] Tous les Forgerons ont quitté l'arène.\n");
    // Phase de nettoyage et sortie propre
    cerbere_cleanup_all_shm(global_pool);
    cerbere_shutdown();
    shm_unlink(shm_control_name);
    return EXIT_SUCCESS;
}
