/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 07/06/2026
 *
 * brique3_sim  :
 * utilisation :
 *
 * =============================================================================*/
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#define PORT 6969 /* Port d'écoute nominal de Chronos */
#define BUFFER_SIZE (64 * 1024) /* 64 Ko pour accueillir le RLE max */

static volatile sig_atomic_t g_running = 1;

/* * Structure du paquet de demande (Request Packet)
 * Alignement strict, pas de padding. Taille totale = 8 octets.
 */
typedef struct __attribute__((packed)) {
    uint32_t macro_chunk_id; /* ID unique du Macro-Chunk */
    uint8_t mc_x;            /* Coordonnée locale Micro-Chunk X [0..19] */
    uint8_t mc_y;            /* Coordonnée locale Micro-Chunk Y [0..119] */
    uint8_t mc_z;            /* Coordonnée locale Micro-Chunk Z [0..19] */
    uint8_t lod;             /* Niveau de détail : 0, 1 ou 2 */
} ChunkRequestPacket;

typedef union {
    struct {
        int16_t x;
        int16_t z;
    } coord;
    uint32_t id;
} MacroKey;

// Interception du Ctrl+C pour fermer la connexion proprement
void handle_sigint(int sig) {
    if (sig == SIGINT) {
        g_running = 0;
    }
}

int main() {
int sock = 0;
    struct sockaddr_in serv_addr;
    ChunkRequestPacket request;
    uint8_t recv_buffer[BUFFER_SIZE];
    struct timespec start_time, end_time;

    // Configuration du signal
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    printf("=== ZYNTHAR v0.1 - SIMULATEUR B3 CONNEXION PERSISTANTE ===\n");

    // 1. Initialisation de la Socket TCP Unique
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("[-] Erreur de création de la socket");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("[-] Adresse invalide");
        close(sock);
        return -1;
    }

    // 2. Établissement de la connexion permanente
    printf("[*] Connexion permanente à Chronos (localhost:%d)...", PORT);
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("\n[-] Échec de la connexion. Chronos est-il en ligne ?");
        close(sock);
        return -1;
    }
    printf(" Connecté.\n\n");
    printf("[*] Entrée dans la boucle interactive. Quitter avec Ctrl+C.\n\n");

    // 3. Boucle principale persistante
    while (g_running) {
        int macro_x, macro_z;
        int x, y, z, lod_val;

        printf("--- Nouvelle Demande Chunks ---\n");
        printf("[?] Coordonnée Macro-Chunk X [0..2047] : ");
        if (scanf("%d", &macro_x) != 1) {
            if (errno == EINTR) break; // Sortie si Ctrl+C pendant le scanf
            printf("Saisie invalide.\n");
            while(getchar() != '\n'); // Purge du tampon
            continue;
        }

        printf("[?] Coordonnée Macro-Chunk Z [0..1023] : ");
        if (scanf("%d", &macro_z) != 1) { if (errno == EINTR) break; printf("Saisie invalide.\n"); while(getchar() != '\n'); continue; }

        printf("[?] Coordonnée Micro-Chunk X [0..19]   : ");
        if (scanf("%d", &x) != 1) { if (errno == EINTR) break; printf("Saisie invalide.\n"); while(getchar() != '\n'); continue; }

        printf("[?] Coordonnée Micro-Chunk Y [0..119]  : ");
        if (scanf("%d", &y) != 1) { if (errno == EINTR) break; printf("Saisie invalide.\n"); while(getchar() != '\n'); continue; }

        printf("[?] Coordonnée Micro-Chunk Z [0..19]   : ");
        if (scanf("%d", &z) != 1) { if (errno == EINTR) break; printf("Saisie invalide.\n"); while(getchar() != '\n'); continue; }

        printf("[?] Niveau de LOD [0..2]               : ");
        if (scanf("%d", &lod_val) != 1) { if (errno == EINTR) break; printf("Saisie invalide.\n"); while(getchar() != '\n'); continue; }

        // Validation rapide
        if (macro_x < 0 || macro_x >= 2048 || macro_z < 0 || macro_z >= 1024 ||
            x < 0 || x > 19 || y < 0 || y > 119 || z < 0 || z > 19 || lod_val < 0 || lod_val > 2) {
            printf("[-] Erreur : Une ou plusieurs coordonnées sont hors-limites Zynthar.\n\n");
            continue;
        }

        // Forgeage de la clé et du paquet
        MacroKey key;
        key.coord.x = (int16_t)macro_x;
        key.coord.z = (int16_t)macro_z;

        request.macro_chunk_id = htonl(key.id);
        request.mc_x = (uint8_t)x;
        request.mc_y = (uint8_t)y;
        request.mc_z = (uint8_t)z;
        request.lod  = (uint8_t)lod_val;

        // ------------------------------------------------------------------
        // ENVOI & CHRONOMÉTRAGE HAUTE RÉSOLUTION
        // ------------------------------------------------------------------
        printf("[*] Expédition du paquet réseau...\n");
        
        // Enclenchement du chrono juste avant send
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        
        if (send(sock, &request, sizeof(request), 0) < 0) {
            perror("[-] Erreur d'envoi (serveur déconnecté ?)");
            break;
        }

        // Attente synchrone de la réponse sur le même tunnel
        ssize_t bytes_received = read(sock, recv_buffer, BUFFER_SIZE);
        
        // Arrêt du chrono immédiatement après la lecture de la socket
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        // ------------------------------------------------------------------

        if (bytes_received < 0) {
            perror("[-] Erreur de lecture");
            break;
        } else if (bytes_received == 0) {
            printf("[-] Le serveur Chronos a clos la connexion permanente.\n");
            break;
        } else {
            // Calcul de la différence de temps en microsecondes (µs)
            long diff_sec = end_time.tv_sec - start_time.tv_sec;
            long diff_nsec = end_time.tv_nsec - start_time.tv_nsec;
            double elapsed_us = (double)(diff_sec * 1000000) + ((double)diff_nsec / 1000.0);

            printf("[+] RÉUSSITE : Flux binaire reçu (%ld octets).\n", bytes_received);
            printf("[⏱️] LATENCE ALLER-RETOUR : %.2f µs ( soit %.4f ms )\n", elapsed_us, elapsed_us / 1000.0);
            printf("    ├── Début Hex : ");
            int dump_len = (bytes_received > 8) ? 8 : bytes_received;
            for(int i = 0; i < dump_len; i++) printf("%02X ", recv_buffer[i]);
            printf("\n--------------------------------\n\n");
        }
    }

    // Sortie propre
    printf("\n[!] Signal de fermeture détecté. Libération du tunnel réseau...\n");
    close(sock);
    printf("[+] Simulateur arrêté proprement.\n");
    return 0;
}
