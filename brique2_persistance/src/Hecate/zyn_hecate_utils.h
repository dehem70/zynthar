#ifndef ZYN_HECATE_UTILS_H
#define ZYN_HECATE_UTILS_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_hecate_utils.h
 * Date    : 13/06/2026
 * ============================================================================= */
 
#include "zyn_hecate.h"
#include <stddef.h>
#include <zynthar.h>
 // Pointeur vers le tableau de 32 régions en mémoire partagée
extern HecateRegion* g_hecate_regions;
extern size_t g_hecate_l1_shm_size;

// Pointeur global vers le Pool de la Couche 2
extern HecateLevel2Node* g_hecate_layer2_pool;
extern size_t g_hecate_l2_shm_size;



int zyn_hecate_init_layer1(void);
void zyn_hecate_clean_shm(void);
int zyn_hecate_init_layer2(uint32_t exact_complex_count);

void get_db_path(char *dest, const char *db_name);
uint32_t  zyn_hecate_couche1_from_db(const char* db_path);
uint32_t  zyn_hecate_couche2_from_db(const char* db_path);
static inline uint32_t hecate_l2_pack(uint8_t material, uint32_t index);





#endif // ZYN_HECATE_UTILS_H
