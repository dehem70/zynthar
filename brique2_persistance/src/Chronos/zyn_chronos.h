#ifndef ZYN_CHRONOS_H
#define ZYN_CHRONOS_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_chronos.h
 * Date    : 07/06/2026
 * ============================================================================= */

/**
 * @brief Initialise les handles SQLite3 en ouvrant les bases depuis le Ramdisk
 * et applique les PRAGMA de performance optimisés.
 * @return 0 en cas de succès, -1 si une erreur critique survient.
 */
int chronos_init(void);

/**
 * @brief Ferme proprement toutes les connexions SQLite3 actives.
 */
void chronos_shutdown(void);

/* * Structure du paquet de demande (Request Packet)
 * Alignement strict, calqué sur le référentiel et le simulateur B3.
 */
typedef struct __attribute__((packed)) {
    uint32_t macro_chunk_id;
    uint8_t mc_x;
    uint8_t mc_y;
    uint8_t mc_z;
    uint8_t lod;
} ChunkRequestPacket;


#endif // ZYN_CHRONOS_H
