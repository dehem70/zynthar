/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 31/05/2026
 *
 * zyn_gen_map_river  : Implémentation du traceur hydrographique hybride.
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
#include <stdbool.h>

#include <zynthar.h>

#include "zyn_gen_map_river.h"
#include "zyn_noise.h"
/* Capacité maximale de la file statique d'inondation pour éviter tout malloc */
#define MAX_LAKE_QUEUE 2048

/**
 * @brief Algorithme d'inondation rigoureux (Water Level Search).
 * Inonde l'intégralité de la dépression hydrographique au niveau d'eau horizontal
 * avant de déterminer le véritable col de débordement global (Spillpoint).
 */
static bool zyn_river_flood_sink(MacroChunk* map, int32_t width_x, int32_t depth_z, 
                                 int32_t start_cx, int32_t start_cz, 
                                 int32_t* out_exit_cx, int32_t* out_exit_cz) {
    
    int32_t queue_x[MAX_LAKE_QUEUE];
    int32_t queue_z[MAX_LAKE_QUEUE];
    int32_t q_head = 0;
    int32_t q_tail = 0;

    size_t lake_indices[MAX_LAKE_QUEUE];
    int32_t lake_count = 0;

    /* Enfilement de la source de la cuvette */
    queue_x[q_tail] = start_cx;
    queue_z[q_tail] = start_cz;
    q_tail++;

    size_t start_idx = (size_t)start_cz * width_x + start_cx;
    lake_indices[lake_count++] = start_idx;
    
    /* Le niveau de l'eau initial est égal à l'altitude du fond du trou */
    float niveau_eau_lac = DM_TO_M(map[start_idx].elevation_max_dm);

    const int32_t dx8[] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    const int32_t dz8[] = { -1, -1, 0, 1, 1, 1, 0, -1 };

    int32_t col_global_x = -1;
    int32_t col_global_z = -1;
    float alt_col_globale = 99999.0f;
    bool a_touche_ocean = false;

    /* PASSE 1 : Expansion totale du lac à niveau d'eau horizontal constant */
    while (q_head < q_tail && lake_count < MAX_LAKE_QUEUE - 16) {
        int32_t cur_x = queue_x[q_head];
        int32_t cur_z = queue_z[q_head];
        q_head++;

        for (int32_t v = 0; v < 8; v++) {
            int32_t vx = cur_x + dx8[v];
            int32_t vz = cur_z + dz8[v];

            if (vx <= 0 || vx >= width_x - 1 || vz <= 0 || vz >= depth_z - 1) continue;

            size_t v_idx = (size_t)vz * width_x + vx;
            float alt_v = DM_TO_M(map[v_idx].elevation_max_dm);

            /* Si un voisin direct est la mer, l'exutoire absolu est trouvé.
               Mais on continue de chercher si d'autres bras du lac touchent l'océan */
            if (alt_v <= ZYN_SEA_LEVEL) {
                a_touche_ocean = true;
                col_global_x = vx;
                col_global_z = vz;
                alt_col_globale = ZYN_SEA_LEVEL;
                continue; 
            }

            /* Vérification d'unicité dans le registre du lac */
            bool deja_submerge = false;
            for (int32_t i = 0; i < lake_count; i++) {
                if (lake_indices[i] == v_idx) {
                    deja_submerge = true;
                    break;
                }
            }

            if (!deja_submerge) {
                /* CORRECTION SÉMANTIQUE : 
                   Si l'altitude du voisin est inférieure ou égale au niveau actuel de notre lac,
                   l'eau s'y déverse obligatoirement (Loi des vases communicants). 
                   Le lac s'étend de ce côté de la plaine, et on l'ajoute à la file d'exploration. */
                if (alt_v <= niveau_eau_lac + 1.5f) { 
                    queue_x[q_tail] = vx;
                    queue_z[q_tail] = vz;
                    q_tail++;
                    lake_indices[lake_count++] = v_idx;
                    
                    /* Si le terrain s'enfonce encore plus bas ailleurs, le niveau max du lac s'ajuste */
                    if (alt_v > niveau_eau_lac) {
                        niveau_eau_lac = alt_v;
                    }
                } else {
                    /* C'est la frontière terrestre haute (le rivage sec). 
                       On enregistre le point le plus bas de TOUTE la frontière pour trouver le col de débordement unique */
                    if (!a_touche_ocean && alt_v < alt_col_globale) {
                        alt_col_globale = alt_v;
                        col_global_x = vx;
                        col_global_z = vz;
                    }
                }
            }
        }
    }

    /* PASSE 2 : Application matérielle et figeage de la nappe d'eau douce */
    if (col_global_x != -1) {
        *out_exit_cx = col_global_x;
        *out_exit_cz = col_global_z;

        /* On sature l'entièreté des chunks validés dans le bassin hydrographique */
        for (int32_t i = 0; i < lake_count; i++) {
            map[lake_indices[i]].biome = 255;
        }
        return true;
    }

    return false;
}

void zyn_gen_map_river(MacroChunk* map, int32_t width_x, int32_t depth_z, uint32_t* out_macro_flux_grid) {
    if (map == NULL || width_x <= 0 || depth_z <= 0 || out_macro_flux_grid == NULL) return;

    size_t total_macro = (size_t)width_x * (size_t)depth_z;
    memset(out_macro_flux_grid, 0, total_macro * sizeof(uint32_t));

    const int32_t dx8[] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    const int32_t dz8[] = { -1, -1, 0, 1, 1, 1, 0, -1 };

    const int32_t rayon_exclusion = 16;

    for (int32_t mz = rayon_exclusion; mz < depth_z - rayon_exclusion; mz++) {
        for (int32_t mx = rayon_exclusion; mx < width_x - rayon_exclusion; mx++) {
            size_t m_idx = (size_t)mz * width_x + mx;
            MacroChunk* chunk = &map[m_idx];

            if (chunk->elevation_max_dm <= ZYN_SEA_LEVEL) continue;
            
            if (chunk->biome == 255) continue;

            bool est_isolee = (map[m_idx - 1].elevation_max_dm <= 0 && 
                               map[m_idx + 1].elevation_max_dm <= 0 &&
                               map[m_idx - width_x].elevation_max_dm <= 0 && 
                               map[m_idx + width_x].elevation_max_dm <= 0);

            uint32_t hash = (uint32_t)(mx * 73856093 ^ mz * 19349663);
            bool declencher_source = false;

            if (est_isolee) {
                declencher_source = ((hash % 20) == 0); 
            } else {
                if (chunk->elevation_max_dm > M_TO_DM(50.0f)) {
                    declencher_source = ((hash % 1000) < 2); 
                }
            }

            if (declencher_source) {
                bool eau_dans_les_parages = false;

                for (int32_t rz = -rayon_exclusion; rz <= rayon_exclusion && !eau_dans_les_parages; rz++) {
                    int32_t test_z = mz + rz;
                    size_t offset_test_z = (size_t)test_z * width_x;
                    
                    for (int32_t rx = -rayon_exclusion; rx <= rayon_exclusion; rx++) {
                        int32_t test_x = mx + rx;
                        size_t idx_test = offset_test_z + test_x;

                        /* CONDITIONS D'EXCLUSION : 
                           1. Une autre rivière coule déjà ici (out_macro_flux_grid > 0)
                           2. Un lac a été formé ici par une autre cuvette (map.biome == 255) */
                        if (out_macro_flux_grid[idx_test] > 0 || map[idx_test].biome == 255) {
                            eau_dans_les_parages = true;
                            break;
                        }
                    }
                }

                if (eau_dans_les_parages) {
                    declencher_source = false;
                }
            }
            
            
            if (declencher_source) {
                int32_t cx = mx;
                int32_t cz = mz;
                
                uint32_t current_flow = 1;
                int32_t last_dir_x = 0;
                int32_t last_dir_z = 0;
                int32_t pas = 0;

                while (pas < 10000) {
                    if (cx <= 0 || cx >= width_x - 1 || cz <= 0 || cz >= depth_z - 1) break;

                    size_t curr_idx = (size_t)cz * width_x + cx;
                    float alt_c = DM_TO_M(map[curr_idx].elevation_max_dm);
                    
                    if (alt_c <= 0.1f) break; 

                    if (out_macro_flux_grid[curr_idx] > 0 && pas > 1) {
                        out_macro_flux_grid[curr_idx] += current_flow;
                        break;
                    }

                    out_macro_flux_grid[curr_idx] = current_flow;

                    int32_t meilleur_v = -1;
                    float max_pente_val = -99999.0f;

                    float turbulence = zyn_noise2d((float)cx * 0.1f, (float)cz * 0.1f);

                    for (int32_t v = 0; v < 8; v++) {
                        int32_t vx = cx + dx8[v];
                        int32_t vz = cz + dz8[v];

                        float alt_v = DM_TO_M(map[(size_t)vz * width_x + vx].elevation_max_dm);
                        float pente_brute = alt_c - alt_v;

                        if (fabsf(pente_brute) < 0.001f) {
                            float attraction_ocean = zyn_noise2d((float)vx * 0.02f, (float)vz * 0.02f) * 0.02f;
                            pente_brute += attraction_ocean;
                        }

                        if (dx8[v] == last_dir_x && dz8[v] == last_dir_z) {
                            pente_brute += 0.05f;
                        }

                        float influence_directionnelle = (dx8[v] * turbulence + dz8[v] * (1.0f - fabsf(turbulence))) * 0.04f;
                        pente_brute += influence_directionnelle;

                        if (pente_brute > max_pente_val) {
                            max_pente_val = pente_brute;
                            meilleur_v = v;
                        }
                    }

                    /* SI LA RIVIÈRE RENCONTRE UN VÉRITABLE TROU DE RELIEF (CUVETTE) */
                    if (meilleur_v != -1 && max_pente_val > 0.0001f) {
                        /* Écoulement normal */
                        last_dir_x = dx8[meilleur_v];
                        last_dir_z = dz8[meilleur_v];
                        cx += last_dir_x;
                        cz += last_dir_z;
                    } else {
                        /* INTERCEPTION DE CUVETTE : L'eau monte et s'étend */
                        int32_t exit_cx = cx;
                        int32_t exit_cz = cz;

                        if (zyn_river_flood_sink(map, width_x, depth_z, cx, cz, &exit_cx, &exit_cz)) {
                            /* Téléportation déterministe de la rivière au niveau du col trouvé */
                            last_dir_x = exit_cx - cx;
                            last_dir_z = exit_cz - cz;
                            
                            /* Normalisation du vecteur d'orientation post-lac */
                            if (last_dir_x > 1)  last_dir_x = 1;
                            if (last_dir_x < -1) last_dir_x = -1;
                            if (last_dir_z > 1)  last_dir_z = 1;
                            if (last_dir_z < -1) last_dir_z = -1;

                            cx = exit_cx;
                            cz = exit_cz;
                        } else {
                            /* Si échec de la recherche (bassin trop immense), fin de la branche */
                            break;
                        }
                    }

                    current_flow += 2;
                    pas++;
                }
            }
        }
    }
}
