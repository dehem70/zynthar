#ifndef ZYN_FORGERON_UTILS_H
#define ZYN_FORGERON_UTILS_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_forgeron_utils.h
 * Date    : 11/06/2026
 * ============================================================================= */
/* =============================================================================
 * SOUS-ROUTINES INTERNES DE LA TURBINE DE CALCUL (DÉPORTÉES DE FORGERON_WORK)
 * ============================================================================= */

#include "zyn_b2_memory_pool.h"

/**
 * @brief Tente de réserver un ticket de lecture de manière atomique sur le Ring Buffer.
 * @return uint64_t Le numéro de ticket obtenu, ou la valeur sentinelle (uint64_t)-1 en cas d'échec (concurrence).
 */
uint64_t forgeron_try_reserve_ticket(AtroposRingBuffer *queue);

/**
 * @brief Extrait un NanoJob du Ring Buffer en respectant le séquençage Vyukov
 * et libère immédiatement le slot pour les cycles futurs d'Atropos.
 */
NanoJob forgeron_extract_job(AtroposRingBuffer *queue, uint64_t ticket);

/**
 * @brief Gère la comptabilité atomique de la page SHM et notifie Atlas si le lot est clos.
 */
void forgeron_finalize_job_lifecycle(SharedMemoryPoolHeader *pool, NanoJob job, int event_fd, uint64_t ticket, pid_t my_pid);
/**
 * @brief Configure la non-mise en mémoire tampon des flux d'I/O (Unbuffered)
 */
void forgeron_init_io_buffers(void);

/**
 * @brief Valide les arguments passés par Cerbère au démarrage du processus
 */
void forgeron_validate_arguments(int argc, char *argv[], char **shm_name, int *event_fd);

/**
 * @brief Gère la connexion et le mapping virtuel de la mémoire partagée maîtresse
 */
SharedMemoryPoolHeader* forgeron_map_shared_memory(const char *shm_name);


#endif // ZYN_FORGERON_UTILS_H
