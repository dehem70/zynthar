/* =============================================================================
 *
 * ZYNTHAR v0.1
 *
 * Auteur : Dehem70
 * Date   : 28/05/2026
 *
 * zyn_gen_map_macro : Intégration global
 * pour l'intégralité de la chaîne géomorphologique macro (Relief, Climat, Image).
 * Sauvegarde un rapport persistant dans /reports/benchmarks/
 *
 * =============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

#include <zynthar.h>
#include "zyn_noise.h"
#include "zyn_gen_map_relief.h"
#include "zyn_gen_png.h"
#include "zyn_gen_map_temperature.h"
#include "zyn_gen_map_humidity.h"
#include "zyn_gen_map_biome.h"
#include "zyn_gen_map_macro.h"
#include "zyn_test_framework.h"
#include "zyn_river_system_pipeline.h"

int zyn_gen_map_macro(MacroChunk* map, uint32_t seed, ZynRiverNode** out_flux_grid, int32_t* out_nodes_count, ZynTestConfig* test_config) {
    
    if (map==NULL) return 1;
    
    clock_t start_global, end_global, start_etape, end_etape;
    start_global = clock();
    
    int32_t width_x = ZYN_WORLD_MACRO_WIDTH_X;
    int32_t depth_z = ZYN_WORLD_MACRO_DEPTH_Z;
    size_t total_chunks = (size_t)width_x * (size_t)depth_z;

    PRINT_BOTH("=====================================================================\n");
    PRINT_BOTH("       ZYNTHAR - SQUELETTE PROCÉDURAL MACRO-CONTINENTAL v0.1.0       \n");
    PRINT_BOTH("=====================================================================\n");
    PRINT_BOTH("[CONFIG] Dimensions du Monde : %d x %d Macro-Chunks\n",width_x, depth_z);
    PRINT_BOTH("[CONFIG] Graine du Monde (SEED) : %u\n", seed);
    PRINT_BOTH("=====================================================================\n\n");
    ZynTestConfig config;
    if(test_config==NULL) {
        config.active_test = 0;   
        config.target_step = 8;
        config.stress_runs = 20;  
        config.early_exit = 0;
        config.with_rivers=1;
    } else {
        config=*test_config;
    }
    
    /* =========================================================================
     * PHASE 1/5 : GÉNÉRATION DU RELIEF CONTINENTAL
     * ========================================================================= */
    if (config.early_exit != 1) {
        PRINT_BOTH("[1/5] Sculpture des continents et archipels (45%% Terres)... ");
        fflush(stdout);
        start_etape = clock();
        zyn_gen_map_relief(map, width_x, depth_z, seed,&config);
        end_etape = clock();
        PRINT_BOTH("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);
           
     } 
    /* =========================================================================
     * PHASE 2/5 : GÈNERATION DU MODÈLE THERMIQUE GLOBALE
     * ========================================================================= */
    if (config.early_exit != 1) {
        PRINT_BOTH("[2/5] Application du gradient thermique et insolations... ");
        fflush(stdout);
        start_etape = clock();
        zyn_gen_map_temperature(map, width_x, depth_z, &config);
        end_etape = clock();
        PRINT_BOTH("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);
           
     } 
    /* =========================================================================
     * PHASE 3/5 : SIMULATION HYDRO-CLIMATIQUE (HUMIDITÉ ET VENTS)
     * ========================================================================= */
    if (config.early_exit != 1) {
        PRINT_BOTH("[3/5] Simulation des vents et humidité orographique... ");
        fflush(stdout);
        start_etape = clock();
        zyn_gen_map_humidity(map, width_x, depth_z,&config);
        end_etape = clock();
        PRINT_BOTH("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);
    }
    /* =========================================================================
     * PHASE 4/5 : CALCUL MODÈLE HYDROGRAPHIQUE MACRO CHUNK (112 passes/s !)
     * ========================================================================= */
    ZynRiverNode* temporary_grid = NULL;
    if (config.early_exit != 1 && config.with_rivers==1) {
        PRINT_BOTH("[4/5] Tracé du réseau hydrographique et inondation des fjords... ");
        fflush(stdout);
        start_etape = clock();
        temporary_grid=zyn_generate_all_rivers(map, seed, out_nodes_count);
        end_etape = clock();
        PRINT_BOTH("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);
    }
    if (out_flux_grid != NULL) {
        *out_flux_grid = temporary_grid;
    }
    /* =========================================================================
     * PHASE 5/5 : ATTRIBUTION LOGIQUE DES BIOMES ET EXPORT CHROMATIQUE
     * ========================================================================= */
    if (config.early_exit != 1) {
        PRINT_BOTH("[5/5] Évaluation de la matrice de Whittaker et classification... ");
        fflush(stdout);

        start_etape = clock();
        zyn_gen_map_biome(map, width_x, depth_z,&config);
        end_etape = clock();
        PRINT_BOTH("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);
    }
    /* =========================================================================
     * EXPORTATION DES RÉSULTATS VISUELS SUR DISQUE
     * ========================================================================= */
    PRINT_BOTH("\n[EXPORT] Enregistrement des images PNG d'audit macro... \n");
    
    char img_filename[128];
    sprintf(img_filename, "carte_relief_seed_%u_pas_%i.png", seed,config.target_step);
    if (zyn_gen_png_elevation(map, width_x, depth_z, img_filename)) {
        PRINT_BOTH("  -> %s exportée avec succès.\n",img_filename);
    }
    if (config.target_step>=5) {
        sprintf(img_filename, "carte_temperature_seed_%u_pas_%i.png", seed,config.target_step);
        if (zyn_gen_png_temperature(map, width_x, depth_z, img_filename)) {
            PRINT_BOTH("  -> %s exportée avec succès.\n",img_filename);
        }
    }
    if (config.target_step>=6) {
        sprintf(img_filename, "carte_humidite_seed_%u_pas_%i.png", seed,config.target_step);
        if (zyn_gen_png_humidite(map, width_x, depth_z, img_filename)) {
            PRINT_BOTH("  -> %s exportée avec succès.\n",img_filename);
        }
    }
    if (config.with_rivers == 1 && config.target_step >= 7) {
        sprintf(img_filename, "carte_rivieres_seed_%u_pas_%i.png", seed, config.target_step);
        if (zyn_gen_png_rivers(map, width_x, depth_z, temporary_grid, *out_nodes_count, img_filename)) {
            PRINT_BOTH("  -> %s exportée avec succès.\n", img_filename);
        }
    }
    if (config.target_step>=8) {
            sprintf(img_filename, "carte_biome_seed_%u_pas_%i.png", seed,config.target_step);
        if (zyn_gen_png_biomes(map, width_x, depth_z, img_filename)) {
            PRINT_BOTH("  -> %s exportée avec succès.\n",img_filename);
        }
    }
    end_global = clock();
    double temps_total = ((double)(end_global - start_global)) / CLOCKS_PER_SEC;
    PRINT_BOTH("\n=====================================================================\n");
    PRINT_BOTH(" GÉNERATION MACRO ENREGISTRÉE EN %.4f SECONDES\n", temps_total);
    PRINT_BOTH("=====================================================================\n");
    
    if (config.active_test == 1) {
        
        // RECONSTRUCTION TEMPORAIRE D'UN BUFFER FLOTTANT POUR NOTRE INSTRUMENT DE MESURE
        float* buffer_analyse = (float*)malloc(total_chunks * sizeof(float));
        if (buffer_analyse != NULL) {
            for (size_t i = 0; i < total_chunks; i++) {
                buffer_analyse[i] = (float)map[i].elevation_max_dm;
            }

            // Seuil adapté : puisque la valeur max est 2048, un saut de > 100 est une rupture
            float seuil_rupture = 2500.0f; 
            char step_label[64];
            sprintf(step_label, "SEED_%u", seed);
            sprintf(step_label, "Pas d'arret : %i",config.target_step);
                
            /* Appel de l'outil de continuité */
            zyn_test_verify_continuity(buffer_analyse, width_x, depth_z, seuil_rupture, seed, step_label);
           
            free(buffer_analyse);
        }
    }
    if (config.active_test == 1 && config.target_step>=5) {
        
        // RECONSTRUCTION TEMPORAIRE D'UN BUFFER FLOTTANT POUR NOTRE INSTRUMENT DE MESURE
        float* buffer_analyse = (float*)malloc(total_chunks * sizeof(float));
        if (buffer_analyse != NULL) {
            for (size_t i = 0; i < total_chunks; i++) {
                buffer_analyse[i] = (float)RAW_TO_FLOAT(map[i].temperature_raw);
            }

            float seuil_rupture = 2.0f; 
            char step_label[64];
            sprintf(step_label, "SEED_%u", seed);
            sprintf(step_label, "Pas d'arret : %i",config.target_step);
                
            /* Appel de l'outil de continuité */
            zyn_test_verify_continuity(buffer_analyse, width_x, depth_z, seuil_rupture, seed, step_label);
           
            free(buffer_analyse);
        }
    }
    
    if (config.active_test == 1 && config.target_step==6) {
        
        // RECONSTRUCTION TEMPORAIRE D'UN BUFFER FLOTTANT POUR NOTRE INSTRUMENT DE MESURE
        float* buffer_analyse = (float*)malloc(total_chunks * sizeof(float));
        if (buffer_analyse != NULL) {
            for (size_t i = 0; i < total_chunks; i++) {
                buffer_analyse[i] = (float)(map[i].biome);
            }

            float seuil_rupture = 2.0f; 
            char step_label[64];
            sprintf(step_label, "SEED_%u", seed);
            sprintf(step_label, "Pas d'arret : %i",config.target_step);
                
            /* Appel de l'outil de continuité */
            zyn_test_verify_continuity(buffer_analyse, width_x, depth_z, seuil_rupture, seed, step_label);
           
            free(buffer_analyse);
        }
    }

    return 0;
}
