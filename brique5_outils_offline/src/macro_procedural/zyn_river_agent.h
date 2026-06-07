#ifndef ZYN_RIVER_AGENT_H
#define ZYN_RIVER_AGENT_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_river_agent.h
 * Date    : 05/06/2026
 * ============================================================================= */
#include <stdint.h>
#include <stdbool.h>
#include <zynthar.h>

#define MAX_AGENTS        5000

// Structure de l'Agent Marcheur Hydrographique (interne au générateur offline)
typedef struct {
    int32_t id;
    int32_t parent_id;
    int32_t x;           
    int32_t z;           
    uint32_t water_qty;   /* Remplacé par un entier pour s'aligner avec flow_volume */
    uint8_t last_dir;
    bool is_active;      
} RiverAgent;

// Fonctions de gestion du système d'agents
void river_system_init(void);
RiverAgent* river_agent_spawn(int32_t x, int32_t z, uint32_t initial_water);
void river_agent_kill(RiverAgent* agent);
int32_t river_get_active_count(void);
RiverAgent* river_get_agent_pool(void);

// Fonctions d'accès à la grille de marquage des rivières en RAM
int32_t river_grid_get_id(int32_t x, int32_t z);
void river_grid_set(int32_t x, int32_t z, int32_t river_id, uint32_t flow);
void river_grid_add_flow(int32_t x, int32_t z, uint32_t flow);

uint32_t river_grid_get_flow(int32_t x, int32_t z);
RiverAgent* river_get_agent_by_id(int32_t id);

uint32_t pack_river_data(uint32_t volume, uint8_t direction);
uint32_t unpack_volume(uint32_t packed_data);
uint8_t unpack_direction(uint32_t packed_data);

#endif // ZYN_RIVER_AGENT_H
