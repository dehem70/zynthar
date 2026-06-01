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
#include "zyn_gen_map_river.h"
#include "zyn_gen_map_biome.h"

#define PRINT_BOTH(fmt, ...) do { printf(fmt, ##__VA_ARGS__); } while(0)

int main(int argc, char** argv) {
    clock_t start_global, end_global, start_etape, end_etape;
    start_global = clock();

    /* =========================================================================
     * INITIALISATION DE LA SEED DYNAMIQUE
     * ========================================================================= */
    uint32_t seed = (uint32_t)time(NULL);

    /* Permettre de passer une seed spécifique en argument pour le debug/reproductibilité */
    if (argc > 1) {
        seed = (uint32_t)strtoul(argv[1], NULL, 10);
    }

    PRINT_BOTH("=====================================================================\n");
    PRINT_BOTH("       ZYNTHAR - SQUELETTE PROCÉDURAL MACRO-CONTINENTAL v0.1.0       \n");
    PRINT_BOTH("=====================================================================\n");
    PRINT_BOTH("[CONFIG] Dimensions du Monde : %d x %d Macro-Chunks\n",(ZYN_WORLD_X_MAX/ZYN_MACRO_CHUNK_DIM_M ), (ZYN_WORLD_Z_MAX/ZYN_MACRO_CHUNK_DIM_M ));
    PRINT_BOTH("[CONFIG] Graine du Monde (SEED) : %u\n", seed);
    PRINT_BOTH("=====================================================================\n\n");

    int32_t width_x = ZYN_WORLD_X_MAX/ZYN_MACRO_CHUNK_DIM_M ;
    int32_t depth_z = ZYN_WORLD_Z_MAX/ZYN_MACRO_CHUNK_DIM_M ;
    size_t total_chunks = (size_t)width_x * (size_t)depth_z;

    MacroChunk* map = (MacroChunk*)malloc(total_chunks * sizeof(MacroChunk));
    if (map == NULL) {
        fprintf(stderr, "Erreur fatale : Allocation de la grille Macro échouée.\n");
        return 1;
    }

    /* Initialisation structurelle brute */
    for (int32_t z = 0; z < depth_z; z++) {
        for (int32_t x = 0; x < width_x; x++) {
            size_t idx = (size_t)z * width_x + x;
            map[idx].chunk_x = x;
            map[idx].chunk_z = z;
            map[idx].elevation_max_dm = 0;
            map[idx].temperature_raw = 0;
            map[idx].biome = 0;
        }
    }

    /* =========================================================================
     * PHASE 1/5 : GÉNÉRATION DU RELIEF CONTINENTAL
     * ========================================================================= */
    PRINT_BOTH("[1/5] Sculpture des continents et archipels (45%% Terres)... ");
    fflush(stdout);
    
    start_etape = clock();
    zyn_gen_map_relief(map, width_x, depth_z, seed);
    end_etape = clock();
    PRINT_BOTH("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);
    
      
    /* =========================================================================
     * PHASE 2/5 : GÈNERATION DU MODÈLE THERMIQUE GLOBALE
     * ========================================================================= */
    PRINT_BOTH("[2/5] Application du gradient thermique et insolations... ");
    fflush(stdout);
    start_etape = clock();
    zyn_gen_map_temperature(map, width_x, depth_z);
    end_etape = clock();
    PRINT_BOTH("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);

    /* =========================================================================
     * PHASE 3/5 : SIMULATION HYDRO-CLIMATIQUE (HUMIDITÉ ET VENTS)
     * ========================================================================= */
    PRINT_BOTH("[3/5] Simulation des vents et humidité orographique... ");
    fflush(stdout);
    start_etape = clock();
    zyn_gen_map_humidity(map, width_x, depth_z);
    end_etape = clock();
    PRINT_BOTH("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);

    /* =========================================================================
     * PHASE 4/5 : CALCUL MODÈLE HYDROGRAPHIQUE MACRO CHUNK (112 passes/s !)
     * ========================================================================= */
    PRINT_BOTH("[4/5] Tracé du réseau hydrographique et inondation des fjords... ");
    fflush(stdout);

    size_t total_chunks_monde = (size_t)width_x * (size_t)depth_z;
    uint32_t* flux_grid = (uint32_t*)calloc(total_chunks_monde, sizeof(uint32_t));

    start_etape = clock();
    zyn_gen_map_river(map, width_x, depth_z, flux_grid);
    end_etape = clock();
    PRINT_BOTH("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);

    /* =========================================================================
     * PHASE 5/5 : ATTRIBUTION LOGIQUE DES BIOMES ET EXPORT CHROMATIQUE
     * ========================================================================= */
    PRINT_BOTH("[5/5] Évaluation de la matrice de Whittaker et classification... ");
    fflush(stdout);

    start_etape = clock();
    zyn_gen_map_biome(map, width_x, depth_z);
    end_etape = clock();
    PRINT_BOTH("OK (%.4f sec)\n", ((double)(end_etape - start_etape)) / CLOCKS_PER_SEC);

    /* =========================================================================
     * EXPORTATION DES RÉSULTATS VISUELS SUR DISQUE
     * ========================================================================= */
    PRINT_BOTH("\n[EXPORT] Enregistrement des images PNG d'audit macro... \n");
    
    if (zyn_gen_png_elevation(map, width_x, depth_z, "carte_relief.png")) {
        PRINT_BOTH("  -> 'carte_relief.png' exportée avec succès.\n");
    }
    if (zyn_gen_png_temperature(map, width_x, depth_z, "carte_temperature.png")) {
        PRINT_BOTH("  -> 'carte_temperature.png' exportée avec succès.\n");
    }
    if (zyn_gen_png_rivers(map, width_x, depth_z, flux_grid, "carte_rivieres.png")) {
        PRINT_BOTH("  -> 'carte_rivieres.png' (Fleuves & Fjords) exportée avec succès.\n");
    }
    if (zyn_gen_png_biomes(map, width_x, depth_z, "carte_biomes.png")) {
        PRINT_BOTH("  -> 'carte_biomes.png' COULEURS RGB exportée avec succès !\n");
    }

    free(flux_grid);

    end_global = clock();
    double temps_total = ((double)(end_global - start_global)) / CLOCKS_PER_SEC;
    PRINT_BOTH("\n=====================================================================\n");
    PRINT_BOTH(" GÉNERATION MACRO ENREGISTRÉE EN %.4f SECONDES\n", temps_total);
    PRINT_BOTH("=====================================================================\n");

    free(map);
    return 0;
}
