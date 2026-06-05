#ifndef ZYN_RIVER_STEPPER_H
#define ZYN_RIVER_STEPPER_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_river_stepper.h
 * Date    : 05/06/2026
 * ============================================================================= */

#include "zyn_river_agent.h"
#include <zynthar.h>


// Exécute un pas de simulation pour l'ensemble des agents actifs
// Modifie la grille de suivi et prépare les structures de nœuds
void river_system_step(MacroChunk* world_map,int32_t current_tick);

#endif // ZYN_RIVER_STEPPER_H
