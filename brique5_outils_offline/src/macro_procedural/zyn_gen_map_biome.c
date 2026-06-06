/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 31/05/2026
 *
 * zyn_gen_map_biome  : Implémentation de la Lookup Table de Whittaker et des surcharges physiques.
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
#include "zyn_gen_map_biome.h"

void zyn_gen_map_biome(MacroChunk* map, int32_t width_x, int32_t depth_z,ZynTestConfig* test_config) {
    if (map == NULL || width_x <= 0 || depth_z <= 0) return;

    /*--------------------------------------------------------------------------
     * MATRICE DE WHITTAKER STATIQUE (8x8 Octets)
     * Ligne = Température (0: Polaire -> 7: Équatorial)
     * Colonne = Humidité (0: Aride -> 7: Hyper-Humide)
     *--------------------------------------------------------------------------*/
    static const uint8_t TABLE_WHITTAKER[8][8] = {
        /* H0         H1         H2         H3         H4         H5         H6         H7 */
        { ZYN_BIOME_TOUNDRA, ZYN_BIOME_TOUNDRA, ZYN_BIOME_GLACIER, ZYN_BIOME_GLACIER, ZYN_BIOME_GLACIER, ZYN_BIOME_GLACIER, ZYN_BIOME_GLACIER, ZYN_BIOME_GLACIER }, /* T0 */
        { ZYN_BIOME_TOUNDRA, ZYN_BIOME_TOUNDRA, ZYN_BIOME_TAIGA,    ZYN_BIOME_TAIGA,    ZYN_BIOME_TAIGA,    ZYN_BIOME_TAIGA,    ZYN_BIOME_GLACIER, ZYN_BIOME_GLACIER }, /* T1 */
        { ZYN_BIOME_PLAINE,  ZYN_BIOME_PLAINE,  ZYN_BIOME_TAIGA,    ZYN_BIOME_TAIGA,    ZYN_BIOME_TAIGA,    ZYN_BIOME_TAIGA,    ZYN_BIOME_TAIGA,   ZYN_BIOME_TAIGA    }, /* T2 */
        { ZYN_BIOME_PLAINE,  ZYN_BIOME_PLAINE,  ZYN_BIOME_PLAINE,   ZYN_BIOME_FORET,    ZYN_BIOME_FORET,    ZYN_BIOME_FORET,    ZYN_BIOME_FORET,   ZYN_BIOME_FORET    }, /* T3 */
        { ZYN_BIOME_DESERT,  ZYN_BIOME_PLAINE,  ZYN_BIOME_PLAINE,   ZYN_BIOME_FORET,    ZYN_BIOME_FORET,    ZYN_BIOME_FORET,    ZYN_BIOME_FORET,   ZYN_BIOME_FORET    }, /* T4 */
        { ZYN_BIOME_DESERT,  ZYN_BIOME_DESERT,  ZYN_BIOME_PLAINE,   ZYN_BIOME_FORET,    ZYN_BIOME_FORET,    ZYN_BIOME_FORET,    ZYN_BIOME_JUNGLE,  ZYN_BIOME_JUNGLE   }, /* T5 */
        { ZYN_BIOME_DESERT,  ZYN_BIOME_DESERT,  ZYN_BIOME_DESERT,   ZYN_BIOME_PLAINE,   ZYN_BIOME_FORET,    ZYN_BIOME_JUNGLE,   ZYN_BIOME_JUNGLE,  ZYN_BIOME_JUNGLE   }, /* T6 */
        { ZYN_BIOME_DESERT,  ZYN_BIOME_DESERT,  ZYN_BIOME_DESERT,   ZYN_BIOME_PLAINE,   ZYN_BIOME_JUNGLE,   ZYN_BIOME_JUNGLE,   ZYN_BIOME_JUNGLE,  ZYN_BIOME_JUNGLE   }  /* T7 */
    };

    size_t total_chunks = (size_t)(ZYN_TOTAL_MACRO_CHUNKS);
    MacroChunk* chunk = map;

    /* Seuils d'altitudes convertis en décimètres pour les calculs d'étagement */
    const int16_t alt_abysse_dm  = M_TO_DM(0.8f*ZYN_WORLD_Y_MIN);
    const int16_t alt_profond_dm = M_TO_DM(0.2f*ZYN_WORLD_Y_MIN);
    const int16_t alt_plage_dm   = M_TO_DM(10.0f);
    const int16_t alt_alpin_dm   = M_TO_DM(0.65f*ZYN_WORLD_Y_MAX);
    const int16_t alt_glacier_dm = M_TO_DM(0.8f*ZYN_WORLD_Y_MAX);

    for (size_t i = 0; i < total_chunks; i++) {
        int16_t alt_dm = chunk->elevation_max_dm;
        uint8_t raw_humidity = chunk->biome; /* Rappel : notre buffer d'humidité offline */

        /*----------------------------------------------------------------------
         * REGLE 1 : LE DOMAINE MARITIME BATHYMÉTRIQUE
         *----------------------------------------------------------------------*/
        if (alt_dm <= ZYN_SEA_LEVEL) {
            if (alt_dm <= alt_abysse_dm) {
                chunk->biome = ZYN_BIOME_ABYSSE;
            } else if (alt_dm <= alt_profond_dm) {
                chunk->biome = ZYN_BIOME_EAU_PROFONDE;
            } else {
                chunk->biome = ZYN_BIOME_EAU_COTIERE;
            }
            chunk++;
            continue;
        }
        
        /*----------------------------------------------------------------------
         * REGLE 1.5 : ANCIENS LACS CÔTIERS RABAISSÉS (FJORDS / ESTUAIRES)
         *----------------------------------------------------------------------*/
        if (raw_humidity == 253) {
            chunk->biome = ZYN_BIOME_EAU_COTIERE;
            chunk++;
            continue;
        }

        /*----------------------------------------------------------------------
         * REGLE 2 : LE DOMAINE HYDROGRAPHIQUE INTERIEUR (SURCHARGE SACRÉE LAC)
         *----------------------------------------------------------------------*/
        if (raw_humidity == 255) {
            chunk->biome = ZYN_BIOME_EAU_INTERIEURE;
            chunk++;
            continue;
        }

        /*----------------------------------------------------------------------
         * REGLE 3 : LE DOMAINE ALPIN (HAUTE ALTITUDE ET NEIGES)
         *----------------------------------------------------------------------*/
        if (alt_dm > alt_alpin_dm) {
            /* Si très haut ou s'il fait froid (T < 60), le pic est enneigé */
            if (alt_dm > alt_glacier_dm || chunk->temperature_raw < 60) {
                chunk->biome = ZYN_BIOME_PIC_ENNEIGE;
            } else {
                chunk->biome = ZYN_BIOME_MONTAGNE_ROCHEUSE;
            }
            chunk++;
            continue;
        }

        /*----------------------------------------------------------------------
         * REGLE 4 : LE DOMAINE DE TRANSITION LITTORALE (PLAGE)
         *----------------------------------------------------------------------*/
        if (alt_dm <= alt_plage_dm && chunk->temperature_raw > 40) {
            chunk->biome = ZYN_BIOME_PLAGE;
            chunk++;
            continue;
        }

        /*----------------------------------------------------------------------
         * REGLE 5 : INTERROGATION CLIMATIQUE DE LA MATRICE DE WHITTAKER O(1)
         *----------------------------------------------------------------------*/
        /* Division par 32 branchless via décalage de bits (0-255 -> 0-7) */
        uint8_t t_idx = chunk->temperature_raw >> 5; 
        uint8_t h_idx = raw_humidity >> 5;

        /* Sécurités anti-débordement d'index */
        if (t_idx > 7) t_idx = 7;
        if (h_idx > 7) h_idx = 7;

        chunk->biome = TABLE_WHITTAKER[t_idx][h_idx];
        chunk++;
    }
    if (test_config != NULL && test_config->active_test == 1 && test_config->target_step == 8) {
        test_config->early_exit=1;
    }
}
