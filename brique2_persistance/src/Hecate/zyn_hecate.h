#ifndef ZYN_HECATE_H
#define ZYN_HECATE_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_hecate.h
 * Date    : 13/06/2026
 * ============================================================================= */

#include <stdint.h>
#include <zynthar.h>


#define HECATE_SHM_NAME_L1 "/zyn_hecate_l1"
#define HECATE_SHM_NAME_L2 "/zyn_hecate_l2"
#define HECATE_SHM_NAME_L3 "/zyn_hecate_l3"
#define HECATE_SHM_NAME_POOL3  "/zyn_hecate_pool3"

// Les masques binaires de notre Bit-Packing de Couche 2
#define HECATE_L2_MAT_MASK   0xFF000000 
#define HECATE_L2_INDEX_MASK 0x00FFFFFF

typedef struct {
    uint8_t matiere;
} HecateLevel2Node;

// Niveau 1 : Le nœud pour un étage
typedef struct {
    uint32_t level2_index;
} HecateMacroNode;

// Une colonne verticale (24 octets, alignement parfait)
typedef struct {
    HecateMacroNode etages[ZYN_WORLD_MACRO_HEIGHT_Y];
} HecateColumn;

// Une région : Un tableau plat et CONTINU de 256 * 256 colonnes
// Taille d'une région en RAM : 256 * 256 * 24 octets = 1 572 864 octets (~1.5 Mo)
typedef struct {
    HecateColumn colonnes[ZYN_WORLD_MACRO_DIM * ZYN_WORLD_MACRO_DIM];
} HecateRegion;

#endif // ZYN_HECATE_H
