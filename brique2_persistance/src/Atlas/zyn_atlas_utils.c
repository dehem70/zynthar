/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 12/06/2026
 *
 * zyn_atlas_utils  :
 * utilisation :
 *
 * =============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <mqueue.h>
#include <string.h>

#include <zynthar.h>
#include "zyn_atlas_utils.h"
#include "zyn_chronos_utils.h"
#include "zyn_b2_memory_pool.h" 

static uint8_t g_atlas_local_buffer[PAGE_SIZE_16MO];

void atlas_init_io_buffers(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
}

void atlas_validate_arguments(int argc, char *argv[], char **shm_name, int *event_fd) {
    if (argc < 3) {
        fprintf(stderr, "[❌ ATLAS] Erreur critique : Arguments manquants (SHM + EventFD requis).\n");
        exit(EXIT_FAILURE);
    }
    *shm_name = argv[1];
    *event_fd = atoi(argv[2]);
}

/**
 * @brief Compresse une page de voxels bruts en RLE (Run-Length Encoding) 16 bits.
 * * @param src                  Pointeur vers les 16 Mo de voxels bruts (dans la SHM).
 * @param src_size             Taille brute d'origine (16777216 octets).
 * @param dest                 Pointeur vers le tampon local privé d'Atlas.
 * @param max_dest_size        Taille maximale tolérée pour le tampon dest (ex: 16777216).
 * @return uint32_t            La taille finale en octets du BLOB compressé, 
 * ou src_size si la compression a provoqué une expansion.
 */
 #include <stdint.h>
#include <stddef.h>

uint32_t atlas_compress_rle(const uint8_t *restrict src, uint32_t src_size, 
                            uint8_t *restrict dest, uint32_t max_dest_size) {
    if (__builtin_expect(src_size == 0, 0)) return 0;

    uint32_t write_idx = 0;
    uint32_t read_idx = 0;

    // Débit maximal via vectorisation agressive et réduction des sauts conditionnels
    while (__builtin_expect(read_idx < src_size, 1)) {
        uint8_t current_voxel = src[read_idx];
        
        // Expansion SWAR (SIMD within a register)
        uint64_t target64 = current_voxel;
        target64 |= (target64 << 8);
        target64 |= (target64 << 16);
        target64 |= (target64 << 32);

        uint32_t run_count = 0;

        // Élimination des embranchements multiples : boucle dédiée au traitement 64-bit non-aligné (Fast Path)
        // La limite stricte à 248 permet d'éviter la double vérification (run_count + 8 <= 255) dans le loop unrollé
        while (read_idx + 8 <= src_size && run_count <= 247) {
            uint64_t check64;
            // __builtin_memcpy force GCC à générer un load non-aligné optimal (ex: vmovdqu/movups) sans violation d'aliasing strict
            __builtin_memcpy(&check64, src + read_idx, 8);

            if (check64 != target64) {
                // Détection de la position exacte du bit changeant via Bit Scan Forward / Count Trailing Zeros
                // Évite le fallback lent octet par octet en cas de rupture de run
                uint64_t diff = check64 ^ target64;
                uint32_t matching_bytes = __builtin_ctzll(diff) >> 3;
                read_idx += matching_bytes;
                run_count += matching_bytes;
                goto write_packet;
            }
            read_idx += 8;
            run_count += 8;
        }

        // Reliquat rapide (Scalar Clean-up) sans chevauchement de buffer 64 bits
        while (read_idx < src_size && run_count < 255 && src[read_idx] == current_voxel) {
            read_idx++;
            run_count++;
        }

write_packet:
        // Indication à GCC que ce cas d'échec est hautement improbable (Predictor Hint)
        if (__builtin_expect(write_idx + 2 >= max_dest_size, 0)) {
            return src_size; 
        }

        // Vectorisation de l'écriture : Écrit le couple [run, voxel] en une seule opération 16 bits
        uint16_t packet = (uint16_t)(run_count | (current_voxel << 8));
        __builtin_memcpy(dest + write_idx, &packet, 2);
        write_idx += 2;
    }

    return write_idx;
}
 
 /*
uint32_t atlas_compress_rle(const uint8_t *src, uint32_t src_size, uint8_t *dest, uint32_t max_dest_size) {
    if (src_size == 0) return 0;

    uint32_t write_idx = 0;
    uint32_t read_idx = 0;

    while (read_idx < src_size) {
        uint8_t current_voxel = src[read_idx];
        uint32_t run_count = 0;

        // 🎯 1. Pré-calcul du motif magique 64 bits pour le voxel actuel
        // Si current_voxel = 0x00 -> target64 = 0x0000000000000000
        // Si current_voxel = 0x01 -> target64 = 0x0101010101010101
        uint64_t target64 = current_voxel;
        target64 |= (target64 << 8);
        target64 |= (target64 << 16);
        target64 |= (target64 << 32);

        // 🎯 2. Boucle de détection de la longueur de la plage (Run)
        while (read_idx < src_size && run_count < 255) {
            
            // Étape Éclair : Si on a au moins 8 octets à lire, qu'on ne va pas dépasser la limite de 255,
            // et que l'adresse est bien alignée (ou supportée par le CPU), on compare 8 octets d'un coup !
            if (read_idx + 8 <= src_size && (run_count + 8) <= 255) {
                uint64_t check64 = *(const uint64_t *)(src + read_idx);
                if (check64 == target64) {
                    read_idx += 8;
                    run_count += 8;
                    continue; // On continue à foncer par blocs de 8 octets !
                }
            }

            // Mode de repli (Fallback) : Octet par octet si le motif change ou si on sature les 255
            if (src[read_idx] == current_voxel) {
                read_idx++;
                run_count++;
            } else {
                break; // Le voxel a changé, on ferme le run actuel
            }
        }

        // 🎯 3. Écriture du couple [Répétition, Valeur]
        if (write_idx + 2 >= max_dest_size) {
            return src_size; // Expansion détectée, abandon immédiat
        }
        dest[write_idx++] = (uint8_t)run_count;
        dest[write_idx++] = current_voxel;
    }

    return write_idx; // Renvoie la taille compressée finale
}*/

SharedMemoryPoolHeader* atlas_map_shared_memory(const char *shm_name) {
    int ctrl_fd = shm_open(shm_name, O_RDWR, 0666);
    if (ctrl_fd == -1) {
        perror("[❌ ATLAS] Erreur de connexion au segment SHM");
        exit(EXIT_FAILURE);
    }

    SharedMemoryPoolHeader *pool = (SharedMemoryPoolHeader*)mmap(
        NULL, sizeof(SharedMemoryPoolHeader), PROT_READ | PROT_WRITE, MAP_SHARED, ctrl_fd, 0
    );
    close(ctrl_fd);

    if (pool == MAP_FAILED) {
        perror("[❌ ATLAS] Échec critique du mmap global");
        exit(EXIT_FAILURE);
    }
    return pool;
}

mqd_t atlas_open_chronos_queue(void) {
    mqd_t chronos_mq = mq_open(ZYN_CHRONOS_RECV_MQ_NAME, O_WRONLY);
    if (chronos_mq == (mqd_t)-1) {
        perror("[❌ ATLAS] Échec d'ouverture de la MQ de retour Chronos");
        exit(EXIT_FAILURE);
    }
    return chronos_mq;
}

int atlas_setup_epoll(int event_fd) {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("[❌ ATLAS] Échec de la création de l'instance epoll");
        exit(EXIT_FAILURE);
    }

    struct epoll_event ev;
    ev.events = EPOLLIN; // Réveil sur niveau de données disponibles (EventFD signé)
    ev.data.fd = event_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event_fd, &ev) == -1) {
        perror("[❌ ATLAS] Échec de l'association de l'eventfd à epoll");
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    return epoll_fd;
}
/*
void atlas_compress_and_signal_page(SharedMemoryPoolHeader *pool, int32_t idx, mqd_t chronos_mq) {
    uint8_t *voxels_bruts_shm = chronos_get_and_map_page(pool, idx);

    // 1. Appel de la turbine RLE vers notre tampon privé local (alloué une seule fois par Atlas)
    uint32_t taille_finale = atlas_compress_rle(voxels_bruts_shm, PAGE_SIZE_16MO, g_atlas_local_buffer, PAGE_SIZE_16MO);

    // 2. Application de la règle de décision implicite sur la taille
    if (taille_finale < PAGE_SIZE_16MO) {
        // 🎯 NOMINAL : Compression efficace ! 
        // On écrase le début de la SHM par le BLOB RLE compressé
        memcpy(voxels_bruts_shm, g_atlas_local_buffer, taille_finale);
    
        // On met à jour la taille compressée réelle dans le header du contexte
        pool->nodes[idx].context.compressed_size = taille_finale;
    
       // printf("[📦 ATLAS] RLE Succès : %u octets transférés (Gain : %.2f%%).\n", taille_finale, (1.0f - ((float)taille_finale / PAGE_SIZE_16MO)) * 100.0f);
    } 
    else {
        // 🎯 DÉBRAYAGE : Données trop complexes, le RLE a été avorté ou a provoqué une expansion
        // On laisse les 16 Mo bruts intacts dans la SHM.
        // On écrit la taille brute exacte comme sémaphore implicite pour le client !
        pool->nodes[idx].context.compressed_size = PAGE_SIZE_16MO;
    
        printf("[⚠️ ATLAS] Débrayage RLE : Données trop chaotiques. Préservation du mode BRUT (16 Mo).\n");
    } 

#if ZYN_LOG_DEBUG
    printf("[🌍 ATLAS] Page %d compressée avec succès (%d octets).\n", idx, pool->nodes[idx].context.compressed_size);
#endif

    // 🏁 BASCULEMENT DE L'INTERRUPTEUR DE RETOUR (Cast sécurisé pour IWYU/Clang)
    __atomic_store_n((uint8_t *)&pool->nodes[idx].context.status, ZYN_STATUS_COMPRESSED, __ATOMIC_RELEASE);
    
    // Notification asynchrone renvoyée à la turbine d'écriture de Chronos
    int node_idx = idx;
    if (mq_send(chronos_mq, (const char*)&node_idx, sizeof(node_idx), 0) == -1) {
        fprintf(stderr, "[⚠️ ATLAS] Échec mq_send pour le nœud %d (Queue pleine ?)\n", idx);
    }
}*/
void atlas_compress_and_signal_page(SharedMemoryPoolHeader *restrict pool, int32_t idx, const mqd_t chronos_mq) {
    // Récupération directe du pointeur restrict pour éviter le double calcul d'offset
    uint8_t *restrict voxels_bruts_shm = chronos_get_and_map_page(pool, idx);

    // 1. Compression RLE directe
    uint32_t taille_finale = atlas_compress_rle(voxels_bruts_shm, PAGE_SIZE_16MO, g_atlas_local_buffer, PAGE_SIZE_16MO);

    // Correction du type explicite (C standard n'inclut pas 'auto' pour l'inférence de type comme en C++)
    MicroChunkContext *restrict node_context = &pool->nodes[idx].context;

    // 2. Branchement hautement prédictible
    if (__builtin_expect(taille_finale < PAGE_SIZE_16MO, 1)) {
        __builtin_memcpy(voxels_bruts_shm, g_atlas_local_buffer, taille_finale);
        node_context->compressed_size = taille_finale;
    } 
    else {
        node_context->compressed_size = PAGE_SIZE_16MO;
        printf("[⚠️ ATLAS] Débrayage RLE : Données trop chaotiques. Préservation du mode BRUT (16 Mo).\n");
    } 

#if ZYN_LOG_DEBUG
    printf("[🌍 ATLAS] Page %d compressée avec succès (%d octets).\n", idx, node_context->compressed_size);
#endif

    // 🏁 Libération atomique (Release) de l'état pour les threads consommateurs
    __atomic_store_n(&node_context->status, ZYN_STATUS_COMPRESSED, __ATOMIC_RELEASE);
    
    // Notification non-bloquante optimisée
    const int node_idx = idx;
    if (__builtin_expect(mq_send(chronos_mq, (const char*)&node_idx, sizeof(int), 0) == -1, 0)) {
        fprintf(stderr, "[⚠️ ATLAS] Échec mq_send pour le nœud %d (Queue pleine ?)\n", idx);
    }
}
