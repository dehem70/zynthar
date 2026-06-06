/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 05/06/2026
 *
 * zyn_river_agent  :
 * utilisation :
 *
 * =============================================================================*/
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

// En-têtes requis sous Linux/POSIX pour manipuler 'struct stat' et 'mkdir'
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <zynthar.h>
#include "zyn_river_agent.h"

static RiverAgent agent_pool[MAX_AGENTS];
static int32_t next_agent_id = 0;

// Grille de marquage rapide en RAM pour éviter les conflits d'agents
// Stocke l'ID de la rivière. -1 = Vide.
static int32_t river_id_grid[ZYN_WORLD_MACRO_WIDTH_X * ZYN_WORLD_MACRO_DEPTH_Z] ;
static uint32_t river_flow_grid[ZYN_WORLD_MACRO_WIDTH_X * ZYN_WORLD_MACRO_DEPTH_Z] ;

void river_system_init(void) {
    memset(agent_pool, 0, sizeof(agent_pool));
    next_agent_id = 1;

    // Initialisation des grilles de tracking à l'état vierge
    for (int32_t i = 0; i < ZYN_WORLD_MACRO_WIDTH_X * ZYN_WORLD_MACRO_DEPTH_Z; i++) {
        river_id_grid[i] = -1;
        river_flow_grid[i] = 0;
    }
}
RiverAgent* river_get_agent_by_id(int32_t id) {
    // Si l'ID recherché n'est pas valide ou négatif, on coupe court
    if (id < 0) return NULL;

    // On récupère le pointeur vers le pool d'agents global
    RiverAgent* pool = river_get_agent_pool();

    // On parcourt tout le tableau à la recherche de l'ID
    for (int32_t i = 0; i < MAX_AGENTS; i++) {
        // L'agent doit être actif ET posséder le bon identifiant
        if (pool[i].id == id) {
            return &pool[i]; // On renvoie l'adresse de cet agent
        }
    }

    return NULL; // Si l'agent est mort ou introuvable
}
RiverAgent* river_agent_spawn(int32_t x, int32_t z, uint32_t initial_water) {
    // Sécurité hors-limites du monde
    if (x < 0 || x >= ZYN_WORLD_MACRO_WIDTH_X || z < 0 || z >= ZYN_WORLD_MACRO_DEPTH_Z) {
        return NULL;
    }

    for (int32_t i = 0; i < MAX_AGENTS; i++) {
        if (!agent_pool[i].is_active) {
            agent_pool[i].id = next_agent_id++;
            agent_pool[i].x = x;
            agent_pool[i].z = z;
            agent_pool[i].water_qty = initial_water;
            agent_pool[i].is_active = true;
            return &agent_pool[i];
        }
    }
    return NULL; 
}

void river_agent_kill(RiverAgent* agent) {
    if (agent != NULL) {
        agent->is_active = false;
        agent->water_qty = 0;
    }
}

int32_t river_get_active_count(void) {
    int32_t active = 0;
    for (int32_t i = 0; i < MAX_AGENTS; i++) {
        if (agent_pool[i].is_active) {
            active++;
        }
    }
    return active;
}

RiverAgent* river_get_agent_pool(void) {
    return agent_pool;
}

void river_grid_set(int32_t x, int32_t z, int32_t river_id, uint32_t flow) {
    if (x < 0 || x >= ZYN_WORLD_MACRO_WIDTH_X || z < 0 || z >= ZYN_WORLD_MACRO_DEPTH_Z) return;
    
    size_t idx = (size_t)z * ZYN_WORLD_MACRO_WIDTH_X + x;
    river_id_grid[idx] = river_id;
    river_flow_grid[idx] = flow;
}
void river_grid_set_id(int32_t x, int32_t z, int32_t river_id) {
    // 1. Garde-fou absolu contre les crashs de débordement de carte
    if (x < 0 || x >= ZYN_WORLD_MACRO_WIDTH_X || z < 0 || z >= ZYN_WORLD_MACRO_DEPTH_Z) {
        return; 
    }
    
    // 2. Calcul de l'index linéaire à plat (SANS le coefficient * 3 de l'image)
    size_t idx = (size_t)z * ZYN_WORLD_MACRO_WIDTH_X + x;
    
    // 3. Écriture de l'identifiant de la rivière
    river_id_grid[idx] = river_id;
}

void river_grid_add_flow(int32_t x, int32_t z, uint32_t flow) {
    if (x < 0 || x >= ZYN_WORLD_MACRO_WIDTH_X || z < 0 || z >= ZYN_WORLD_MACRO_DEPTH_Z) return;
    river_flow_grid[z * ZYN_WORLD_MACRO_WIDTH_X + x] += flow;
}

int32_t river_grid_get_id(int32_t x, int32_t z) {
    // 1. Garde-fou anti-débordement : si on cherche en dehors de la carte, 
    // on renvoie -1 (case vide/océan) pour éviter un crash
    if (x < 0 || x >= ZYN_WORLD_MACRO_WIDTH_X || z < 0 || z >= ZYN_WORLD_MACRO_DEPTH_Z) {
        return -1; 
    }
    
    // 2. Calcul de l'index linéaire à plat
    size_t idx = (size_t)z * ZYN_WORLD_MACRO_WIDTH_X + x;
    
    // 3. Renvoi de l'ID stocké à cet endroit (-1 si aucune rivière)
    return river_id_grid[idx];
}

uint32_t river_grid_get_flow(int32_t x, int32_t z) {
    // 1. Garde-fou anti-débordement : si on cherche en dehors de la carte, 
    // on renvoie un débit de 0 pour éviter un crash
    if (x < 0 || x >= ZYN_WORLD_MACRO_WIDTH_X || z < 0 || z >= ZYN_WORLD_MACRO_DEPTH_Z) {
        return 0; 
    }
    
    // 2. Calcul de l'index linéaire à plat
    size_t idx = (size_t)z * ZYN_WORLD_MACRO_WIDTH_X + x;
    
    // 3. Renvoi du débit d'eau stocké dans le tableau de flux
    return river_flow_grid[idx];
}


// Compacter les deux valeurs dans un seul uint32_t
uint32_t pack_river_data(uint32_t volume, uint8_t direction) {
    // 1. On s'assure que les valeurs ne dépassent pas leurs masques par sécurité
    uint32_t v_masked = volume & 0x1FFFF;   // Masque 17 bits (131071 max)
    uint32_t d_masked = direction & 0xF;    // Masque 4 bits (15 max)

    // 2. On décale la direction de 17 bits vers la gauche et on fusionne avec un OR (|)
    return (d_masked << 17) | v_masked;
}

// Extraire le volume depuis le uint32_t compacté
uint32_t unpack_volume(uint32_t packed_data) {
    // On applique un masque pour ne lire que les 17 premiers bits
    return packed_data & 0x1FFFF;
}

// Extraire la direction depuis le uint32_t compacté
uint8_t unpack_direction(uint32_t packed_data) {
    // On décale de 17 bits vers la droite, puis on applique le masque de 4 bits
    return (uint8_t)((packed_data >> 17) & 0xF);
}
