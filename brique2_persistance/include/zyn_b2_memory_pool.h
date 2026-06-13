#ifndef ZYN_B2_MEMORY_POOL_H
#define ZYN_B2_MEMORY_POOL_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_b2_memory_pool.h
 * Date    : 08/06/2026
 * ============================================================================= */
 
 #include <pthread.h>
#include <stdint.h>
#define PAGE_SIZE_16MO 16777216
#define MAX_POOL_PAGES 64 // Limite théorique haute, mais n'occupe aucune RAM au départ

typedef struct {
    uint8_t context_id;          
    uint8_t lod;                 
    uint32_t macro_id;           
    uint8_t mc_x;                
    uint8_t mc_y;                
    uint8_t mc_z;                
    uint8_t biome;               
    uint8_t temperature_raw;     
    int16_t coin_nw; int16_t coin_ne; int16_t coin_sw; int16_t coin_se;
    
    char shm_page_name[32];      // EX : "/zynthar_page_5" -> Le nom du fichier SHM de 16 Mo
    _Atomic int32_t jobs_remaining;
    _Atomic uint8_t status;
    uint32_t compressed_size;
} MicroChunkContext;

typedef struct {
    MicroChunkContext context;
    int32_t next_free_idx;       
    uint8_t is_allocated;        // Flag pour savoir si Cerbère a physiquement créé la page
} PoolNode;


#define NANO_QUEUE_SIZE 16384 // Le nombre de cases sur notre tapis roulant

typedef struct {
    uint64_t ticket_id;
    uint8_t shm_node_idx;  // L'index de la page de 16 Mo dans la SHM maîtresse (0 à 63)
    uint8_t sub_x;         // Index X du Micro-Chunk local (0 à 15)
    uint8_t sub_y;         // Index Y du Micro-Chunk local (0 à 15)
    uint8_t sub_z;         // Index Z du Micro-Chunk local (0 à 15)
} NanoJob;

typedef struct {
    NanoJob buffer[NANO_QUEUE_SIZE]; // NANO_QUEUE_SIZE = 16384 slots
    _Atomic uint64_t head;
    uint64_t tail;         
} AtroposRingBuffer;

typedef struct {
    pthread_mutex_t lock;         
    pthread_cond_t cond_free;     
    int32_t top_idx;             
    int32_t current_count;       
    int32_t low_watermark; 
    int32_t head_idx;       // Tête de la FIFO (Pour le POP de Chronos)
    int32_t tail_idx;
    
    PoolNode nodes[MAX_POOL_PAGES]; 
    AtroposRingBuffer atropos_queue;
    
} SharedMemoryPoolHeader;


// --- CONFIGURATION DE LA MESSAGE QUEUE (CHRONOS -> ATROPOS) ---
#define ZYN_ATROPOS_MQ_NAME "/zyn_atropos_mq"  // Nom système de la file (doit commencer par un /)
#define ZYN_MQ_MAX_MSG      10               // Nombre max de messages en attente dans la file

typedef struct {
    uint8_t shm_node_idx;                      // L'index de la page SHM à découper (0 à 63)
} AtroposMessage;

#define ZYN_CHRONOS_RECV_MQ_NAME "/zyn_chronos_recv_mq"

#endif // ZYN_B2_MEMORY_POOL_H
