#ifndef ZYN_IPC_H
#define ZYN_IPC_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_ipc.h
 * Date    : 16/06/2026
 * ============================================================================= */

#include <stdint.h>
#include <stddef.h>
#define <zynthar.h>

// --- CONSTANTES CONFIGURATION SYSTÈME ---
#define ZYN_SHM_BASE_NAME       "/zynthar_station_"   // Complété par l'ID de la station (ex: /zynthar_station_42)
#define ZYN_VOXELS_PER_MICRO    ZYN_MICRO_CHUNK_TOTAL_VOXELS
#define ZYN_MICRO_PER_MESO      ZYN_MESO_CHUNK_TOTAL_MICRO
#define ZYN_NET_PACKET_MAX_SIZE 65536                 // 64 Ko alloués au maximum pour le produit fini compressé

/**
 * ============================================================================
 * 1. FLUX GÉNÉRAL : FIFOs de 2 Octets (uint16_t)
 * ============================================================================
 * Utilisé pour : FIFO_FREE_STATIONS, FIFO_REQUESTED, FIFO_ANALYSED, 
 * FIFO_FORGED, FIFO_A_NETTOYER, ZYN_STATE_COMPRESSED.
 * * Un simple entier non-signé de 16 bits suffit amplement. L'écriture et la 
 * lecture de 2 octets dans une FIFO sous Linux sont nativement atomiques.
 * * Capacité maximale : 65 536 postes de travail simultanés en mémoire.
 */
typedef uint16_t zyn_fifo_station_msg_t;


/**
 * ============================================================================
 * 2. FLUX FORGERONS : FIFOs de 4 Octets (uint32_t via Union)
 * ============================================================================
 * Utilisé pour : FIFO_MICRO_A_FORGER et FIFO_RETOUR_FORGERONS.
 * * Pour que le message reste léger (4 octets) et son écriture atomique dans la
 * FIFO sans corruption, on utilise une `union`. 
 * Elle permet de manipuler le ticket soit sous forme de champs découpés (struct),
 * soit sous forme d'un entier brut 32 bits (`raw`) pour l'envoi/lecture direct.
 */
typedef union {
    struct {
        uint16_t station_id;  // Bits 0 à 15  : Numéro du poste de travail (0 à 65535)
        uint8_t  micro_idx;   // Bits 16 à 23 : Index du Micro-Chunk dans la table (0 à 63)
        uint8_t  reserved;    // Bits 24 à 31 : Libre / Statut de secours si nécessaire (non utilisé)
    } __attribute__((packed)) fields;

    uint32_t raw;             // Permet l'écriture/lecture atomique directe dans le tube
} zyn_fifo_worker_ticket_t;


/**
 * ============================================================================
 * EXEMPLE THÉORIQUE D'UTILISATION DANS LE CODE :
 * ============================================================================
 * * --- ÉCRITURE PAR ATROPOS (Envoi d'un travail à un forgeron) ---
 * zyn_fifo_worker_ticket_t ticket;
 * ticket.fields.station_id = 42;
 * ticket.fields.micro_idx  = 12;
 * write(fd_fifo_a_forger, &ticket.raw, sizeof(ticket.raw));
 * * --- LECTURE PAR UN FORGERON ---
 * zyn_fifo_worker_ticket_t ticket_recu;
 * if (read(fd_fifo_a_forger, &ticket_recu.raw, sizeof(ticket_recu.raw)) > 0) {
 * // Le forgeron sait instantanément où aller bosser en SHM :
 * uint16_t ma_station = ticket_recu.fields.station_id;
 * uint8_t  mon_bloc   = ticket_recu.fields.micro_idx;
 * }
 */
 
 // --- ÉNUMÉRATIONS DE CONTRÔLE ---

/**
 * @brief États de cycle de vie global d'un Poste de Travail (Section 1).
 */
typedef enum {
    ZYN_STATION_FREE = 0,         // Disponible dans le pool
    ZYN_STATION_ASSIGNED,         // Réservée par le réseau pour un Job
    ZYN_STATION_IN_ANALYSIS,      // Atropos est en train de calculer la géologie
    ZYN_STATION_IN_FORGE,         // Les Forgerons sculptent les Micro-Chunks
    ZYN_STATION_READY_TO_COMPRESS,// Tous les forgerons ont fini, Atlas peut passer
    ZYN_STATION_COMPRESSED        // Paquet prêt en Section 5, le réseau peut l'envoyer
} zyn_station_state_t;

/**
 * @brief États de traitement local d'un Micro-Chunk (Section 3).
 */
typedef enum {
    ZYN_BLOC_HOMOGENE = 0,        // Le bloc est 100% uniforme (Air ou Roche pure)
    ZYN_BLOC_A_FORGER             // Le bloc contient une frontière géologique, sculpture requise
} zyn_bloc_state_t;

/**
 * @brief Indexation nommée et stricte de notre tableau de FIFOs.
 */
typedef enum {
    FIFO_FREE_STATIONS = 0,       // Stations prêtes et désinfectées (2 octets)
    FIFO_A_NETTOYER,              // Stations qui ont fini leur cycle réseau (2 octets)
    FIFO_REQUESTED,               // Envoyé par le Réseau -> Lu par Atropos (2 octets)
    FIFO_ANALYSED,                // Signalement fin géologie générale (2 octets)
    FIFO_MICRO_A_FORGER,          // Envoyé par Atropos -> Lu par Forgerons (4 octets)
    FIFO_RETOUR_FORGERONS,        // Envoyé par Forgerons -> Lu par Atropos (4 octets)
    FIFO_FORGED,                  // Signalement fin de toute sculpture -> Lu par Atlas (2 octets)
    FIFO_COMPRESSED,              // Signalement fin de compression -> Lu par Réseau (2 octets)
    ZYN_FIFO_COUNT                // Donne automatiquement le nombre total de FIFOs (8)
} zyn_fifo_idx_t;

// --- REPERTOIRE DES CHEMINS PHYSIQUES DES FIFOs ---
static const char* const ZYN_FIFO_PATHS[ZYN_FIFO_COUNT] = {
    [FIFO_FREE_STATIONS]    = "/tmp/zyn_fifo_free_stations",
    [FIFO_A_NETTOYER]       = "/tmp/zyn_fifo_a_nettoyer",
    [FIFO_REQUESTED]        = "/tmp/zyn_fifo_requested",
    [FIFO_ANALYSED]         = "/tmp/zyn_fifo_analysed",
    [FIFO_MICRO_A_FORGER]   = "/tmp/zyn_fifo_micro_a_forger",
    [FIFO_RETOUR_FORGERONS] = "/tmp/zyn_fifo_retour_forgerons",
    [FIFO_FORGED]           = "/tmp/zyn_fifo_forged",
    [FIFO_COMPRESSED]       = "/tmp/zyn_fifo_compressed"
};

// ============================================================================
// STRUCTURES DES MESSAGES TRANSITANT DANS LES FIFOS
// ============================================================================

/**
 * @brief Format des messages pour le FLUX GÉNÉRAL (2 octets).
 */
typedef uint16_t zyn_fifo_station_msg_t;

/**
 * @brief Format des messages pour le FLUX FORGERONS (4 octets).
 */
typedef union {
    struct {
        uint16_t station_id;  // Numéro du poste de travail (0 à 65535)
        uint8_t  micro_idx;   // Index du Micro-Chunk dans la table (0 à 63)
        uint8_t  reserved;    // Octet libre/statut (non utilisé par Atropos/Forgeron)
    } __attribute__((packed)) fields;

    uint32_t raw;             // Transfert binaire atomique direct
} zyn_fifo_worker_ticket_t;


// ============================================================================
// STRUCTURE INTERNE D'UN POSTE DE TRAVAIL (FICHIER SHM UNIQUE)
// ============================================================================

/**
 * @brief SECTION 1 : L'En-tête de Contrôle et Routage (64 octets alignés)
 */
typedef struct __attribute__((aligned(64))) {
    uint64_t job_id;                 // ID unique de la requête client / réseau
    uint32_t macro_chunk_id;         // ID du Macro-Chunk parent (256m)
    int32_t  meso_x;                 // Coordonnées relatives du Meso-Chunk (16m)
    int32_t  meso_y;
    int32_t  meso_z;
    volatile uint32_t state;         // Statut global (casté en zyn_station_state_t)
    volatile uint32_t chunks_pending;// Compteur atomique décrémenté par les forgerons
    uint8_t  padding[24];            // Alignement strict sur la ligne de cache L1/L2
} zyn_header_t;

/**
 * @brief SECTION 2 : L'Empreinte Géologique Majeure (Niveau Meso-Chunk - 16m)
 * Alignée à 64 octets pour préserver l'isolation de lecture.
 */
typedef struct __attribute__((aligned(64))) {
    float    altitudes_ref[5];       // Altitudes théoriques : 4 coins + 1 centre
    float    amplitudes_aretes[4];   // 4 Amplitudes verticales modulant le relief et la surface
    uint8_t  biome_majeur_id;        // ID du biome dominant
    uint8_t  biomes_adjacents[4];    // IDs des 4 biomes voisins pour interpolation
    float    facteurs_mixage[4];     // Poids d'influence des biomes adjacents (0.0 à 1.0)
    uint8_t  padding[11];            // Remplissage vers 64 octets
} zyn_geo_meso_t;

/**
 * @brief SECTION 3 : Carte Géologique Locale d'un Micro-Chunk (Niveau 4m)
 */
typedef struct {
    uint64_t materiaux_mask;         // Champ de bits (64 max). Si Homogène, 1 seul bit à 1.
    uint8_t  statut_local;           // Casté en zyn_bloc_state_t (HOMOGENE ou A_FORGER)
    uint8_t  mat_superieur_id;       // ID matériau au-dessus de la frontière (ex: herbe)
    uint8_t  mat_inferieur_id;       // ID matériau en-dessous de la frontière (ex: roche)
    uint8_t  has_limit;              // Indicateur de présence de frontière déterministe
    uint32_t limite_data_1d;         // Paramètres packagés pour l'équation de coupe 1D
} zyn_geo_micro_t;

/**
 * @brief STRUCTURE TOTALE DE LA TABLE DE TRAVAIL (~4,16 Mo)
 */
typedef struct {
    // Section 1 : Dashboard (64 octets)
    zyn_header_t    header;          

    // Section 2 : Macro-Géologie (64 octets)
    zyn_geo_meso_t  geo_meso;        

    // Section 3 : Micro-Géologie (64 blocs * 16 octets = 1024 octets)
    zyn_geo_micro_t geo_micro[ZYN_MICRO_PER_MESO]; 

    // Section 4 : Buffer Volumétrique Brut (64 * 64000 voxels * 1 octet = 4,096 Mo)
    // Indexé par [micro_chunk_idx][voxel_idx]
    uint8_t         voxel_buffer[ZYN_MICRO_PER_MESO][ZYN_VOXELS_PER_MICRO];

    // Section 5 : Zone Réseau / Produit Fini (~64 Ko)
    uint8_t         network_packet[ZYN_NET_PACKET_MAX_SIZE];
    size_t          packet_size;     // Taille réelle écrite par Atlas lors de la compression
} zyn_station_t;
 
 
#endif // ZYN_IPC_H
