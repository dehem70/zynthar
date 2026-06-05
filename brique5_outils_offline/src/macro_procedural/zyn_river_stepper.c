/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 05/06/2026
 *
 * zyn_river_stepper  :
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

#include "zyn_river_stepper.h"
#include "zyn_river_agent.h"
#include <limits.h>

static const int32_t dir_dx[9] = { 0,  0,  1, 1, 1, 0, -1, -1, -1 };
static const int32_t dir_dz[9] = { 0,  1,  1, 0, -1, -1,  -1,  0,  1 }; 

void river_system_step(MacroChunk* world_map, int32_t current_tick) {
    RiverAgent* agents = river_get_agent_pool();

    for (int32_t i = 0; i < MAX_AGENTS; i++) {
        if (!agents[i].is_active) continue;

        RiverAgent* const agent = &agents[i];
        const int32_t current_index = agent->z * ZYN_WORLD_MACRO_WIDTH_X + agent->x;
        const int16_t current_elev = world_map[current_index].elevation_max_dm;

        int32_t best_dir = -1;
        float best_score = -999999.0f; // On passe sur un système de score physique !
        int32_t escape_cuvette_dir = -1;
        int16_t minimal_ascent = SHRT_MAX;

        // 1. Analyse des 8 voisins avec calcul de score (Pente + Inertie + Sinuosité)
        for (int32_t d = 1; d <= 8; d++) {
            const int32_t nx = agent->x + dir_dx[d];
            const int32_t nz = agent->z + dir_dz[d];

            if (nx < 0 || nx >= ZYN_WORLD_MACRO_WIDTH_X || nz < 0 || nz >= ZYN_WORLD_MACRO_DEPTH_Z) continue;
            
            // Interdiction absolue de marcher sur son propre tracé direct pour éviter le sur-place
            if (river_grid_get_id(nx, nz) == agent->id) continue;

            const int32_t neighbor_index = nz * ZYN_WORLD_MACRO_WIDTH_X + nx;
            const int16_t neighbor_elev = world_map[neighbor_index].elevation_max_dm;

            // SÉCURITÉ ESTUAIRE : Si on touche la mer, l'agent a fini son travail !
            if (world_map[neighbor_index].elevation_max_dm<0) {
                river_grid_set(nx, nz, agent->id, agent->water_qty);
                river_agent_kill(agent);
                best_dir = -2; // Marqueur d'arrêt propre
                break;
            }

            // CAS DESCENTE OU PLAT (Écoulement naturel)
            if (neighbor_elev <= current_elev) {
                float slope_force = (float)(current_elev - neighbor_elev);
                
                // FORCE 1 : L'Inertie (Bonus si le voisin est dans la même direction que le tick d'avant)
                float inertia_bonus = 0.0f;
                if (d == agent->last_dir) {
                    inertia_bonus = 40.0f; // Donne de l'élan pour aller tout droit
                }

                // FORCE 2 : La Sinuosité (Légère perturbation pseudo-aléatoire pour créer des méandres)
                // On utilise les coordonnées pour générer une déviation déterministe
                float sinuosity = (float)((nx ^ nz ^ current_tick) % 7) * 4.0f;

                float total_score = slope_force + inertia_bonus + sinuosity;

                if (total_score > best_score) {
                    best_score = total_score;
                    best_dir = d;
                }
            } 
            // CAS CUVETTE : Le voisin monte, mais on stocke la montée la plus faible pour s'échapper si on est bloqué
            else {
                int16_t ascent = neighbor_elev - current_elev;
                if (ascent < minimal_ascent && river_grid_get_id(nx, nz) == -1) {
                    minimal_ascent = ascent;
                    escape_cuvette_dir = d;
                }
            }
        }

        // Si l'agent s'est jeté dans la mer pendant le scan, on passe au suivant
        if (best_dir == -2) continue;

        // 2. Traitement du blocage en Cuvette (Remplissage du lac virtuel)
        bool a_grimpe_cuvette = false;
        if (best_dir == -1) {
            if (escape_cuvette_dir != -1) {
                best_dir = escape_cuvette_dir;
                a_grimpe_cuvette = true; // L'agent "saute" par-dessus le bord du trou
            } else {
                river_agent_kill(agent);
                continue;
            }
        }

        const int32_t next_x = agent->x + dir_dx[best_dir];
        const int32_t next_z = agent->z + dir_dz[best_dir];

        // 3. Gestion des confluences (Si on croise un autre fleuve, on s'y jette)
        int32_t existing_id = river_grid_get_id(next_x, next_z);
        if (existing_id != -1 && existing_id != agent->id) {
            
            // NOUVEAU : On récupère le pointeur de l'agent qui possède cette case
            // (Il te faut une petite fonction ou un accès direct au pool pour vérifier le parent)
            RiverAgent* other_agent = river_get_agent_by_id(existing_id);
            
            // RÈGLE D'IMMUNITÉ : Si la case appartient à notre parent direct 
            // ou si nous sommes le parent de cet agent, ON NE MEURT PAS. 
            // On considère que c'est notre propre delta/division, on a le droit de se croiser !
            if (other_agent != NULL && 
                (other_agent->id == agent->parent_id || other_agent->parent_id == agent->id)) 
            {
                // Immunité familiale : on autorise le chevauchement sans tuer l'agent
                // On peut simplement partager l'espace
            } 
            else {
                // VRAIE CONFLUENCE : C'est un fleuve totalement étranger.
                // On lui donne notre eau et on meurt proprement pour s'y jeter.
                river_grid_add_flow(next_x, next_z, agent->water_qty);
                river_agent_kill(agent);
                continue;
            }
        }
        // 4. Coût de mouvement et usure cinétique
        // Sortir d'une cuvette en grimpant coûte cher (simule le volume d'un lac à remplir)
        uint32_t cout_mouvement = a_grimpe_cuvette ? 250 : 1;
        if (agent->water_qty <= cout_mouvement) {
            river_agent_kill(agent);
            continue;
        } else {
            agent->water_qty -= cout_mouvement;
        }

        
 // 5. Enregistrement du déplacement
        agent->last_dir = (uint8_t)best_dir;
        
        if (a_grimpe_cuvette) {
            // NOUVEAU GARDE-FOU ANTI-LAC SUSPENDU :
            // On vérifie si par hasard l'un des voisins de la case actuelle touche l'océan
            bool proche_ocean = false;
            for (int32_t vd = 1; vd <= 8; vd++) {
                int32_t vx = agent->x + dir_dx[vd];
                int32_t vz = agent->z + dir_dz[vd];
                if (vx >= 0 && vx < ZYN_WORLD_MACRO_WIDTH_X && vz >= 0 && vz < ZYN_WORLD_MACRO_DEPTH_Z) {
                    size_t v_idx = (size_t)vz * ZYN_WORLD_MACRO_WIDTH_X + vx;
                    if (world_map[v_idx].biome == 255 || world_map[v_idx].biome == 253) {
                        proche_ocean = true;
                        break;
                    }
                }
            }

            // Si on est juste à côté de la mer, INTERDICTION de rehausser le relief !
            // On force l'eau à "creuser" un canyon invisible (on ne modifie pas l'altitude)
            if (!proche_ocean) {
                // Logique classique d'aplatissement (uniquement à l'intérieur des terres)
                const int32_t next_index = next_z * ZYN_WORLD_MACRO_WIDTH_X + next_x;
                int16_t target_elev = world_map[next_index].elevation_max_dm;
                
                world_map[current_index].elevation_max_dm = target_elev;

                // Remplissage des voisins directs
                for (int32_t vd = 1; vd <= 8; vd++) {
                    int32_t vx = agent->x + dir_dx[vd];
                    int32_t vz = agent->z + dir_dz[vd];
                    if (vx >= 0 && vx < ZYN_WORLD_MACRO_WIDTH_X && vz >= 0 && vz < ZYN_WORLD_MACRO_DEPTH_Z) {
                        size_t v_idx = (size_t)vz * ZYN_WORLD_MACRO_WIDTH_X + vx;
                        if (world_map[v_idx].elevation_max_dm < target_elev) {
                            world_map[v_idx].elevation_max_dm = target_elev;
                        }
                    }
                }
            } else {
                // Si on est proche de l'océan, l'agent "saute" la falaise sans créer de tache/lac.
                // Au prochain tick, sa boucle détectera l'océan et le tuera proprement via la sécurité estuaire.
            }
        }

        // Déplacement effectif
        agent->x = next_x;
        agent->z = next_z;

        river_grid_set(agent->x, agent->z, agent->id, agent->water_qty);
    }
}
