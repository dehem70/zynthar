#ifndef ZYN_GEN_MAP_MACRO_H
#define ZYN_GEN_MAP_MACRO_H

#include <stdint.h>

// Inclusion globale gérée par CMake
#include <zynthar.h>
#include "zyn_test_framework.h"

int zyn_gen_map_macro(MacroChunk* map, uint32_t seed, ZynRiverNode** out_flux_grid, int32_t* out_nodes_count, ZynTestConfig* test_config);

#endif /* ZYN_GEN_MAP_MACRO_H */
