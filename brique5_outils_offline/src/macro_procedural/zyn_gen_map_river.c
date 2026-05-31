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

static bool zyn_river_flood_sink(MacroChunk* map, int32_t width_x, int32_t depth_z, 
                                 int32_t start_cx, int32_t start_cz, 
                                 int32_t* out_exit_cx, int32_t* out_exit_cz) {
    
    int32_t queue_x[MAX_LAKE_QUEUE];
    int32_t queue_z[MAX_LAKE_QUEUE];
    int32_t q_head = 0;
    int32_t q_tail = 0;

    size_t lake_indices[MAX_LAKE_QUEUE];
    int32_t lake_count = 0;

    queue_x[q_tail] = start_cx;
    queue_z[q_tail] = start_cz;
    q_tail++;

    size_t start_idx = (size_t)start_cz * width_x + start_cx;
    lake_indices[lake_count++] = start_idx;
    
    /* On sauvegarde l'ancien état au cas où on devrait faire un rollback */
    uint8_t ancien_biome_start = map[start_idx].biome;
    map[start_idx].biome = 254; 
    float niveau_eau_lac = DM_TO_M(map[start_idx].elevation_max_dm);

    const int32_t dx8[] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    const int32_t dz8[] = { -1, -1, 0, 1, 1, 1, 0, -1 };

    int32_t col_global_x = -1;
    int32_t col_global_z = -1;
    float alt_col_globale = 99999.0f;
    bool a_touche_ocean = false;

    while (q_head < q_tail && lake_count < MAX_LAKE_QUEUE - 16) {
        int32_t cur_x = queue_x[q_head];
        int32_t cur_z = queue_z[q_head];
        q_head++;

        for (int32_t v = 0; v < 8; v++) {
            int32_t vx = cur_x + dx8[v];
            int32_t vz = cur_z + dz8[v];

            if (vx <= 0 || vx >= width_x - 1 || vz <= 0 || vz >= depth_z - 1) continue;

            size_t v_idx = (size_t)vz * width_x + vx;
            if (map[v_idx].biome == 254) continue; 

            float alt_v = DM_TO_M(map[v_idx].elevation_max_dm);

            if (alt_v <= ZYN_SEA_LEVEL || map[v_idx].biome == 253) {
                a_touche_ocean = true;
                col_global_x = vx;
                col_global_z = vz;
                alt_col_globale = ZYN_SEA_LEVEL;
                continue; 
            }

            /* Absorption des plaines basses ou des morceaux de lacs pré-existants */
            if (alt_v <= niveau_eau_lac + 1.5f || map[v_idx].biome == 255) { 
                queue_x[q_tail] = vx;
                queue_z[q_tail] = vz;
                q_tail++;
                lake_indices[lake_count++] = v_idx;
                
                map[v_idx].biome = 254; 

                if (alt_v > niveau_eau_lac && map[v_idx].biome != 255) {
                    niveau_eau_lac = alt_v;
                }
            } else {
                if (!a_touche_ocean && alt_v < alt_col_globale) {
                    alt_col_globale = alt_v;
                    col_global_x = vx;
                    col_global_z = vz;
                }
            }
        }
    }

    if (col_global_x != -1) {
        *out_exit_cx = col_global_x;
        *out_exit_cz = col_global_z;

        if (a_touche_ocean) {
            for (int32_t i = 0; i < lake_count; i++) {
                size_t idx = lake_indices[i];
                int32_t cx = (int32_t)(idx % width_x);
                int32_t cz = (int32_t)(idx / width_x);

                /* Échantillonnage d'un bruit doux à moyenne fréquence */
                float bruit = zyn_noise2d((float)cx * 0.25f, (float)cz * 0.25f);
                float normalise = (bruit + 1.0f) * 0.5f; /* Ramené entre 0.0 et 1.0 */

                /* On calcule une profondeur en mètres qui ondule doucement entre -1.5m et -9.5m */
                float prof_m = -1.5f - (normalise * 8.0f);

                map[idx].elevation_max_dm = M_TO_DM(prof_m);
                map[lake_indices[i]].biome = 253; 
            }
        } else {
            for (int32_t i = 0; i < lake_count; i++) {
                map[lake_indices[i]].biome = 255; 
            }
        }
        return true;
    }

    /* Rollback de sécurité */
    map[start_idx].biome = ancien_biome_start;
    for (int32_t i = 1; i < lake_count; i++) {
        if (map[lake_indices[i]].biome == 254) map[lake_indices[i]].biome = 0;
    }
    return false;
}

void zyn_gen_map_river(MacroChunk* map, int32_t width_x, int32_t depth_z, uint32_t* out_macro_flux_grid) {
    if (map == NULL || width_x <= 0 || depth_z <= 0 || out_macro_flux_grid == NULL) return;

    size_t total_macro = (size_t)width_x * (size_t)depth_z;
    memset(out_macro_flux_grid, 0, total_macro * sizeof(uint32_t));

    const int32_t dx8[] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    const int32_t dz8[] = { -1, -1, 0, 1, 1, 1, 0, -1 };
    const int32_t rayon_exclusion = 4;

    /* PASSE 1 : Tracé classique des rivières et Sink Filling local */
    for (int32_t mz = rayon_exclusion; mz < depth_z - rayon_exclusion; mz++) {
        for (int32_t mx = rayon_exclusion; mx < width_x - rayon_exclusion; mx++) {
            size_t m_idx = (size_t)mz * width_x + mx;
            MacroChunk* chunk = &map[m_idx];

            if (chunk->elevation_max_dm <= ZYN_SEA_LEVEL || chunk->biome == 255 || chunk->biome == 253) continue;

            bool est_isolee = (map[m_idx - 1].elevation_max_dm <= 0 && 
                               map[m_idx + 1].elevation_max_dm <= 0 &&
                               map[m_idx - width_x].elevation_max_dm <= 0 && 
                               map[m_idx + width_x].elevation_max_dm <= 0);

            uint32_t hash = (uint32_t)(mx * 73856093 ^ mz * 19349663);
            bool declencher_source = false;

            if (est_isolee) {
                declencher_source = ((hash % 20) == 0); 
            } else {
                if (chunk->elevation_max_dm > M_TO_DM(100.0f)) {
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
                        if (out_macro_flux_grid[idx_test] > 0 || map[idx_test].biome == 255 || map[idx_test].biome == 253) {
                            eau_dans_les_parages = true;
                            break;
                        }
                    }
                }
                if (eau_dans_les_parages) declencher_source = false;
            }

            if (declencher_source) {
                int32_t cx = mx;
                int32_t cz = mz;
                uint32_t current_flow = 1;
                int32_t last_dir_x = 0;
                int32_t last_dir_z = 0;
                int32_t pas = 0;

                while (pas < 1000) {
                    if (cx <= 0 || cx >= width_x - 1 || cz <= 0 || cz >= depth_z - 1) break;

                    size_t curr_idx = (size_t)cz * width_x + cx;
                    float alt_c = DM_TO_M(map[curr_idx].elevation_max_dm);
                    
                    if (alt_c <= 0.1f || map[curr_idx].biome == 253) break; 

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
                            pente_brute += zyn_noise2d((float)vx * 0.02f, (float)vz * 0.02f) * 0.02f;
                        }
                        if (dx8[v] == last_dir_x && dz8[v] == last_dir_z) pente_brute += 0.05f;

                        pente_brute += (dx8[v] * turbulence + dz8[v] * (1.0f - fabsf(turbulence))) * 0.04f;

                        if (pente_brute > max_pente_val) {
                            max_pente_val = pente_brute;
                            meilleur_v = v;
                        }
                    }

                    if (meilleur_v != -1 && max_pente_val > 0.0001f) {
                        last_dir_x = dx8[meilleur_v];
                        last_dir_z = dz8[meilleur_v];
                        cx += last_dir_x;
                        cz += last_dir_z;
                    } else {
                        int32_t exit_cx = cx;
                        int32_t exit_cz = cz;
                        if (zyn_river_flood_sink(map, width_x, depth_z, cx, cz, &exit_cx, &exit_cz)) {
                            last_dir_x = exit_cx - cx;
                            last_dir_z = exit_cz - cz;
                            if (last_dir_x > 1)  last_dir_x = 1;
                            if (last_dir_x < -1) last_dir_x = -1;
                            if (last_dir_z > 1)  last_dir_z = 1;
                            if (last_dir_z < -1) last_dir_z = -1;
                            cx = exit_cx;
                            cz = exit_cz;
                        } else {
                            break;
                        }
                    }
                    current_flow += 2;
                    pas++;
                }
            }
        }
    }

    /* =========================================================================
     * PASSE 2 : UNIFICATION ABSOLUE ET CONVERGENCE DES MASSES D'EAU CONNECTÉES
     * =========================================================================
     * On applique un Flood-Fill de nettoyage global ultra-rapide.
     * Si une case 255 (Lac) est collée à une case 253 (Fjord) ou à la Mer, 
     * toute sa nappe est instantanément convertie en Fjord et abaissée à 0.
     * On répète l'opération tant que des modifications ont lieu (généralement 2 à 3 itérations max).
     * ========================================================================= */
    bool modification_active = true;
    while (modification_active) {
        modification_active = false;
        for (int32_t mz = 1; mz < depth_z - 1; mz++) {
            for (int32_t mx = 1; mx < width_x - 1; mx++) {
                size_t idx = (size_t)mz * width_x + mx;
                
                /* Si c'est un lac d'eau douce, on regarde s'il touche un fjord ou l'océan */
                if (map[idx].biome == 255) {
                    bool doit_devenir_fjord = false;
                    for (int32_t v = 0; v < 8; v++) {
                        int32_t vx = mx + dx8[v];
                        int32_t vz = mz + dz8[v];
                        size_t n_idx = (size_t)vz * width_x + vx;

                        if (map[n_idx].biome == 253 || DM_TO_M(map[n_idx].elevation_max_dm) <= ZYN_SEA_LEVEL) {
                            doit_devenir_fjord = true;
                            break;
                        }
                    }

                    if (doit_devenir_fjord) {
                        /* Une case lac s'effondre en fjord par contagion.
                           On calcule son bruit de profondeur localisé de la même manière. */
                        float bruit = zyn_noise2d((float)mx * 0.25f, (float)mz * 0.25f);
                        float prof_m = -1.5f - ((bruit + 1.0f) * 0.5f * 8.0f);

                        map[idx].elevation_max_dm = M_TO_DM(prof_m);
                        map[idx].biome = 253; 
                        modification_active = true;
                    }
                }
            }
        }
    }
}
