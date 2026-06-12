#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// Inclure ici le header qui contient la structure exacte de ton pool (SharedMemoryPoolHeader, etc.)
#include "../../include/zyn_b2_memory_pool.h" 

int main() {
    // 1. Liaison au segment SHM existant (Ajuste le nom du segment si nécessaire)
    int shm_fd = shm_open("/zynthar_shm_pool", O_RDONLY, 0666);
    if (shm_fd == -1) {
        perror("[❌ INSPECT] Impossible d'ouvrir le segment SHM");
        return 1;
    }

    // 2. Mapping en lecture seule pour garantir qu'on ne perturbe rien
    size_t shm_size = sizeof(SharedMemoryPoolHeader);
    SharedMemoryPoolHeader *pool = mmap(NULL, shm_size, PROT_READ, MAP_SHARED, shm_fd, 0);
    if (pool == MAP_FAILED) {
        perror("[❌ INSPECT] Mmap échoué");
        close(shm_fd);
        return 1;
    }

    printf("\n================= INSPECTION DE LA RAM DE L'USINE =================\n");
    printf("[📌 RING BUFFER] Head réelle : %lu | Tail réelle : %lu\n", 
           pool->atropos_queue.head, pool->atropos_queue.tail);
    printf("===================================================================\n\n");

    int anomalies = 0;
    // On scanne les 64 contextes de pages SHM du pool
    for (int i = 0; i < 64; i++) {
        uint8_t status = pool->nodes[i].context.status;
        int32_t remaining = pool->nodes[i].context.jobs_remaining;
        uint64_t macro_id = pool->nodes[i].context.macro_id;

        // Si la page n'est ni FREE (0) ni COMPRESSED/SENT (3) (Ajuste selon tes enums de production)
        if (status != 0 && status != 3) {
            printf("💥 Page SHM [%d] active détectée :\n", i);
            printf("   └── Macro_ID       : %lu\n", macro_id);
            printf("   └── Status actuel  : %d\n", status);
            printf("   └── Jobs Remaining : %d\n", remaining);
            printf("-------------------------------------------------------------------\n");
            anomalies++;
        }
    }

    if (anomalies == 0) {
        printf("[✅ INSPECT] Aucune page active ou bloquée trouvée dans les contextes.\n");
    } else {
        printf("[⚠️ INSPECT] %d page(s) suspecte(s) immobilisée(s) en RAM.\n", anomalies);
    }

    printf("===================================================================\n");

    munmap(pool, shm_size);
    close(shm_fd);
    return 0;
}
