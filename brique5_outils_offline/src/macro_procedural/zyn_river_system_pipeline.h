
#include <stdlib.h>
#include <stdio.h>
#include <zynthar.h>
#include "zyn_river_agent.h"

/**
 
 * @param world_map Pointeur vers ton tableau linéaire de [2000 * 1000] MacroChunks.
 * @param world_seed La graine aléatoire globale du serveur.
 * @param out_nodes_count Pointeur pour récupérer le nombre de nœuds de rivières générés.
 * @return Un tableau dynamique de ZynRiverNode contenant les rivières vectorielles.
 */
ZynRiverNode* zyn_generate_all_rivers(MacroChunk* world_map, uint32_t world_seed, int32_t* out_nodes_count);

