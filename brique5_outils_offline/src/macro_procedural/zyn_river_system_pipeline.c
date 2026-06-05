#include "zyn_river_agent.h"
#include "zyn_river_stepper.h"
#include <stdlib.h>
#include <stdio.h>
#include <zynthar.h>

/**
 * Fonction maîtresse à inclure dans ton générateur de monde offline.
 * @param world_map Pointeur vers ton tableau linéaire de [2000 * 1000] MacroChunks.
 * @param world_seed La graine aléatoire globale du serveur.
 * @param out_nodes_count Pointeur pour récupérer le nombre de nœuds de rivières générés.
 * @return Un tableau dynamique de ZynRiverNode contenant les rivières vectorielles.
 */
// Définition des paramètres de dispersion pour éviter la saturation locale
#define SPATIAL_GRID_STEP  20   // On n'inspecte la côte que toutes les 12 cases pour étaler le spawn
#define BASE_SPAWN_CHANCE  10    // Chance de base (5%) qu'une côte valide devienne un estuaire
#define SEUIL_ALTITUDE_DM    1000    // 800 mètres : altitude minimale pour qu'une source apparaisse
#define EAU_INITIALE_SOURCE  120000  // Volume d'eau colossal pour garantir de longs trajets

ZynRiverNode* zyn_generate_all_rivers(MacroChunk* world_map, uint32_t world_seed, int32_t* out_nodes_count) {
    printf("[Hydro] Initialisation du pipeline de simulation avec dispersion spatiale...\n");
    
    river_system_init();
    srand(world_seed);
    int32_t agents_created = 0;
    uint32_t spawn_seed = world_seed;

    // Générateur déterministe local pour le spawn
    inline uint32_t simple_rand(uint32_t* s) {
        *s = (*s * 1103515245 + 12345) & 0x7fffffff;
        return *s;
    }
    
// On parcourt la carte avec un pas de 6 pour bien répartir les sources sur les 2000 km
    for (int32_t z = SPATIAL_GRID_STEP; z < ZYN_WORLD_MACRO_DEPTH_Z - SPATIAL_GRID_STEP; z += SPATIAL_GRID_STEP) {
        for (int32_t x = SPATIAL_GRID_STEP; x < ZYN_WORLD_MACRO_WIDTH_X - SPATIAL_GRID_STEP; x += SPATIAL_GRID_STEP) {
            
            size_t idx = (size_t)z * ZYN_WORLD_MACRO_WIDTH_X + x;
            
            // SEUIL DES MONTAGNES : On fait naître l'eau sur les hauts plateaux / sommets
            // Par exemple, si l'altitude est supérieure à 12 000 décimètres (1200 mètres)
            if (world_map[idx].elevation_max_dm > SEUIL_ALTITUDE_DM) {
                
                // On vérifie que la case est libre
                if (river_grid_get_id(x, z) == -1) {
                    if ((rand() % 100) < BASE_SPAWN_CHANCE) {
                    // On leur donne une réserve d'eau colossale (100 000 unités)
                        uint32_t base_water = EAU_INITIALE_SOURCE; 
                    
                        RiverAgent* ag = river_agent_spawn(x, z, base_water);
                        if (ag != NULL) {
                            river_grid_set(x, z, ag->id, ag->water_qty);
                            agents_created++;
                        }
                    }
                }
            }
            
            if (agents_created >= MAX_AGENTS) break; // Limite pour ne pas saturer la RAM au début
        }
        if (agents_created >= MAX_AGENTS) break;
    }
    printf("[Hydro] Pipeline initialisé. %d sources créées sur les sommets.\n", agents_created);

    // 3. Boucle logique temporelle synchrone (Inchangée)
    int32_t current_tick = 0;
    while (river_get_active_count() > 0) {
        RiverAgent* pool = river_get_agent_pool();
        
        
        // Avancement des marcheurs
        river_system_step(world_map,current_tick);

        current_tick++;
    }
    
    printf("[Hydro] Simulation hydrographique globale achevée en %d ticks.\n", current_tick);
    
    // 4. Extraction des nœuds (Inchangée)
    int32_t node_count = 0;
    for (int32_t z = 0; z < ZYN_WORLD_MACRO_DEPTH_Z; z++) {
        for (int32_t x = 0; x < ZYN_WORLD_MACRO_WIDTH_X; x++) {
            if (river_grid_get_id(x, z) != -1) node_count++;
        }
    }

    if (node_count == 0) {
        *out_nodes_count = 0;
        return NULL;
    }

    ZynRiverNode* river_vector = malloc(sizeof(ZynRiverNode) * node_count);
    if (river_vector == NULL) {
        *out_nodes_count = 0;
        return NULL;
    }

    int32_t current_node_idx = 0;
    for (int32_t z = 0; z < ZYN_WORLD_MACRO_DEPTH_Z; z++) {
        for (int32_t x = 0; x < ZYN_WORLD_MACRO_WIDTH_X; x++) {
            int32_t river_id = river_grid_get_id(x, z);
            if (river_id != -1) {
                river_vector[current_node_idx].macro_x = x;
                river_vector[current_node_idx].macro_z = z;
                river_vector[current_node_idx].flow_volume = 1000; 
                river_vector[current_node_idx].direction = 1;      
                current_node_idx++;
            }
        }
    }

    *out_nodes_count = node_count; 
    return river_vector; 
}
