/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 07/06/2026
 *
 * brique3_sim : Matraqueur de requêtes asynchrone haute performance
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
#include <pthread.h>

#define PORT 6969 /* Port d'écoute nominal de Chronos */
#define BUFFER_SIZE (600 * 1024) /* ~600 Ko pour gober la page simulée d'Atlas d'un coup */
#define TARGET_STRESS_COUNT 1000 /* Nombre de requêtes à envoyer dans la rafale */

static volatile sig_atomic_t g_running = 1;
static _Atomic uint64_t g_total_sent = 0;
static _Atomic uint64_t g_total_received = 0;

typedef struct __attribute__((packed)) {
    uint32_t macro_chunk_id; /* ID unique du Macro-Chunk */
    uint8_t mc_x;            /* Coordonnée locale Micro-Chunk X [0..19] */
    uint8_t mc_y;            /* Coordonnée locale Micro-Chunk Y [0..119] */
    uint8_t mc_z;            /* Coordonnée locale Micro-Chunk Z [0..19] */
    uint8_t lod;             /* Niveau de détail : 0, 1 ou 2 */
} ChunkRequestPacket;

typedef union {
    struct {
        uint8_t z;    
        uint8_t x;    
        uint8_t rz;   
        uint8_t rx;   
    };
    uint32_t id;     
} Id;

void handle_sigint(int sig) {
    if (sig == SIGINT) {
        g_running = 0;
    }
}

// =============================================================================
// 📥 TURBINE DE RÉCEPTION (Lit les retours de Chronos en tâche de fond)
// =============================================================================
void* response_reader_thread(void *arg) {
    int sock = *(int*)arg;
    uint8_t recv_buffer[BUFFER_SIZE];

    printf("[📥 SIM-READER] Écouteur de réponses actif sur la socket.\n");

uint32_t bytes_expected = 512 * 1024; // 512 Ko attendus par page d'Atlas
    uint32_t current_packet_bytes = 0;

    while (g_running) {
        ssize_t bytes_read = read(sock, recv_buffer, BUFFER_SIZE);

        if (bytes_read > 0) {
            current_packet_bytes += bytes_read;
            
            // Si on a accumulé assez d'octets pour reconstituer une page entière
            if (current_packet_bytes >= bytes_expected) {
                __atomic_fetch_add(&g_total_received, 1, __ATOMIC_RELAXED);
                current_packet_bytes -= bytes_expected; // On garde le reliquat s'il y a chevauchement
            }
        } else if (bytes_read == 0) {
            printf("[📥 SIM-READER] Connexion close par Chronos.\n");
            break;
        } else {
            if (errno == EINTR) continue;
            perror("[❌ SIM-READER] Erreur de lecture");
            break;
        }
    }
    return NULL;
}

// =============================================================================
// 🏁 PROGRAMME PRINCIPAL (Bombardier)
// =============================================================================
int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    struct timespec start_stress, end_stress;

    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    printf("=== ZYNTHAR v0.1 - MATRAQUEUR ASYNCHRONE B3 ===\n");

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

    printf("[*] Connexion au tunnel persistant de Chronos...");
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("\n[-] Connexion impossible");
        close(sock);
        return -1;
    }
    printf(" Connecté.\n\n");

    // Lancement du thread récepteur
    pthread_t reader_tid;
    if (pthread_create(&reader_tid, NULL, response_reader_thread, &sock) != 0) {
        perror("[-] Échec de création du thread de lecture");
        close(sock);
        return -1;
    }

    printf("[🔥 STRESS] Lancement de la salve de %d requêtes consécutives...\n", TARGET_STRESS_COUNT);
    
    // Top chrono du stress test complet
    clock_gettime(CLOCK_MONOTONIC, &start_stress);

    // Initialisation d'une coordonnée de départ pour balayer la DB
    int base_macro_x = 4;
    int base_macro_z = 5;

    for (int i = 0; i < TARGET_STRESS_COUNT && g_running; i++) {
        ChunkRequestPacket request;
        
        // On fait varier légèrement les coordonnées pour forcer SQLite à chercher en DB
        int current_x = base_macro_x + (i % 20); 
        int current_z = base_macro_z + (i % 20);

        Id id;
        id.rx = (uint8_t)(current_x / 256);
        id.rz = (uint8_t)(current_z / 256);
        id.x  = (uint8_t)(current_x - 256 * id.rx);
        id.z  = (uint8_t)(current_z - 256 * id.rz);

        request.macro_chunk_id = htonl(id.id);
        request.mc_x = (uint8_t)(i % 20);
        request.mc_y = (uint8_t)(i % 120);
        request.mc_z = (uint8_t)(i % 20);
        request.lod  = 0;

        // BOMBARDEMENT SANS ATTENDRE
        if (send(sock, &request, sizeof(request), 0) < 0) {
            perror("[-] Erreur d'envoi réseau");
            break;
        }

        __atomic_fetch_add(&g_total_sent, 1, __ATOMIC_RELAXED);
        usleep(1);
    }

    printf("[📢 METRIC] Fin d'envoi des requêtes. Attente de la réception des derniers paquets retour...\n");
    
   int stagnation_counter = 0;
uint64_t last_received = 0;

while (g_total_received < g_total_sent && stagnation_counter < 30) {
    sleep(1); // On attend 1 seconde
    if (g_total_received == last_received) {
        stagnation_counter++; // Si le score ne monte plus, on s'approche de la fin
    } else {
        stagnation_counter = 0; // Ça progresse, on reset le compteur
        last_received = g_total_received;
    }
}
    clock_gettime(CLOCK_MONOTONIC, &end_stress);

    // Arrêt propre du thread de fond
    g_running = 0;
    pthread_cancel(reader_tid);
    pthread_join(reader_tid, NULL);

    // =========================================================================
    // BILAN DU MATRAQUAGE
    // =========================================================================
    long diff_sec = end_stress.tv_sec - start_stress.tv_sec;
    long diff_nsec = end_stress.tv_nsec - start_stress.tv_nsec;
    double total_time_ms = (double)(diff_sec * 1000) + ((double)diff_nsec / 1000000.0);
    
    uint64_t sent = __atomic_load_n(&g_total_sent, __ATOMIC_RELAXED);
    uint64_t rec = __atomic_load_n(&g_total_received, __ATOMIC_RELAXED);
    double iops = (double)rec / (total_time_ms / 1000.0);

    printf("\n================= BILAN DU STRESS TEST =================\n");
    printf("[🚀] Requêtes expédiées      : %lu\n", sent);
    printf("[📥] Réponses récoltées      : %lu\n", rec);
    printf("[⏱️] Durée totale du test     : %.2f ms ( soit %.4f secondes )\n", total_time_ms, total_time_ms / 1000.0);
    printf("[📊] Cadence brute (Débit)   : %.2f IOPS (Requêtes/Sec)\n", iops);
    printf("[💎] Latence moyenne estimée : %.2f µs par chunk\n", (total_time_ms * 1000.0) / (double)rec);
    printf("========================================================\n\n");

    close(sock);
    return 0;
}
