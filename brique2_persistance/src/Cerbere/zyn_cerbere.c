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

// Inclusions des référentiels du projet
#include <zynthar.h>
#include "zyn_cerbere.h"

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

    // 2. 💡 LE CASSE : Renommer le point de montage pour libérer l'espace nominal immédiatement
    // Même si Linux dit "Busy", l'appel système rename() sur un répertoire est atomique.
    char trash_path[1024];
    snprintf(trash_path, sizeof(trash_path), "%s_trash_%d", ZYN_RAMDISK_PATH, (int)getpid());
    
    int renamed = 0;
    if (rename(ZYN_RAMDISK_PATH, trash_path) == 0) {
        fprintf(stdout, "[🔄 CERBÈRE] Point de montage déréférencé vers %s\n", trash_path);
        renamed = 1;
    } else {
        fprintf(stderr, "[⚠️ CERBÈRE] Impossible de déréférencer le dossier : %s. Repli standard.\n", strerror(errno));
    }

    const char *target_to_unmount = renamed ? trash_path : ZYN_RAMDISK_PATH;

    // 3. Démontage synchrone sur la cible
    fprintf(stdout, "[🔌 CERBÈRE] Libération synchrone du point de montage tmpfs...\n");
    if (umount(target_to_unmount) != 0) {
        // Si le umount classique échoue encore, on utilise le mode LAZY pour forcer le kernel à purger
        umount2(target_to_unmount, MNT_DETACH);
    }

    // Un micro-sommeil de sécurité pour laisser le VFS appliquer le détachement
    usleep(10000);

    // 4. Destruction physique du dossier
    if (rmdir(target_to_unmount) == 0) {
        fprintf(stdout, "[🗑️ CERBÈRE] Répertoire détruit proprement.\n");
    } else {
        // Si le code 16 persiste, il est confiné dans le dossier "trash", l'espace nominal est propre !
        fprintf(stdout, "[⚠️ CERBÈRE] Nettoyage en arrière-plan délégué au noyau Linux (Code: %d).\n", errno);
    }

    fprintf(stdout, "[💤 CERBÈRE] Subsystem offline. Atelier Zynthar prêt pour la prochaine session.\n");
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

    // ------------------------------------------------------------------
    // STEP 2 : LANCEMENT INDÉPENDANT DE CHRONOS (WORKER 0)
    // ------------------------------------------------------------------
    printf("[✅ CERBÈRE] Alignement des planètes... Lancement de Chronos (Worker 0).\n");
    
    g_chronos_pid = fork();

    if (g_chronos_pid < 0) {
        perror("[❌ CERBÈRE] Erreur critique lors du fork de Chronos");
        exit(EXIT_FAILURE);
    }

    if (g_chronos_pid == 0) {
        // --- PROCESSUS FILS : ÉXÉCUTION DE CHRONOS ---
        // On remplace l'image du processus courant par l'exécutable de Chronos
        // Assure-toi que le chemin correspond à ton binaire compilé par CMake
        char *args[] = {"./brique2_persistance/zyn_chronos", NULL};
        if (execv(args[0], args) == -1) {
            perror("[❌ CERBÈRE] Erreur critique : Impossible d'exécuter zyn_chronos");
            exit(EXIT_FAILURE);
        }
    }

    // --- PROCESSUS PÈRE : CERBÈRE EN SURVEILLANCE ---
    printf("[✅ CERBÈRE] Chronos démarré avec succès par Cerbère (PID: %d).\n", g_chronos_pid);
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

        // On dort 200ms entre chaque vérification de santé pour ne pas surcharger le CPU
        usleep(200000);
    }
    printf("\n[⚠️ CERBÈRE] Cerbère intercepte l'arrêt global du serveur.\n");
    if (g_chronos_pid > 0) {
        printf("[🛑 CERBÈRE] Attente de la confirmation d'extinction de Chronos...\n");
        waitpid(g_chronos_pid, NULL, 0); // Attente bloquante ici pour être sûr que Chronos a fini ses close()
        printf("[✅ CERBÈRE] Chronos a validé son nettoyage.\n");
    }
    // Phase de nettoyage et sortie propre
    cerbere_shutdown();
    return EXIT_SUCCESS;
}
