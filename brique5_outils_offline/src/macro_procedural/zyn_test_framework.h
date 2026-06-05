#ifndef ZYN_TEST_FRAMEWORK_H
#define ZYN_TEST_FRAMEWORK_H

/* =============================================================================
 * Projet Zynthar v.0.1.0
 * Fichier : zyn_test_framework.h
 * Date    : 01/06/2026
 * ============================================================================= */

#include <stdint.h>

/**
 * @brief Structure de configuration pour piloter les tests procedurales
 */
typedef struct {
    int32_t active_test;  /* 0 = Normal, 1 = Mode Test activé */
    int32_t target_step;  /* 1 = Voronoi, 2 = Perlin, 3 = Fusion, 4 = Centiles */
    int32_t stress_runs;  /* Nombre de cartes/seeds à stress-tester (ex: 20) */
    int32_t early_exit;   /* 1 = S'arrêter immédiatement après le pas cible */
    int32_t with_rivers;
} ZynTestConfig;

/**
 * @brief Analyse la continuité locale d'un buffer flottant.
 * @return Le nombre de pixels présentant une rupture de gradient supérieure au seuil.
 */
int32_t zyn_test_verify_continuity(const float* buffer, int32_t width_x, int32_t depth_z, 
                                   float seuil_rupture, uint32_t seed, const char* step_name);

/**
 * @brief Exécute l'auto-test du thermomètre mathématique (Mock OK / NOK).
 * @return 1 si le framework est validé et fiable, 0 si l'outil de mesure est défaillant.
 */
int32_t zyn_test_calibrate_framework(int32_t width_x, int32_t depth_z);

#endif // ZYN_TEST_FRAMEWORK_H
