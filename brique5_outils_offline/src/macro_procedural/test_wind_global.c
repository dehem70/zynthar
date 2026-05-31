/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 30/05/2026
 *
 * test_wind_global  :
 * utilisation :
 *
 * =============================================================================*/
  
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "wind_global.h"
#include <zynthar.h>

int main() {
    printf("=== RUNNING TESTS FOR WIND_GLOBAL (WITH EXISTING ZYN_NOISE) ===\n");

    // Initialisation du bruit avec une graine de test
    zyn_noise_init(424242U);

    int32_t cx = 200;
    int32_t cy = 150;

    // Test 1 : Déterminisme strict
    WindVector w1 = get_global_wind(cx, cy);
    WindVector w2 = get_global_wind(cx, cy);

    printf("Test 1 (Determinisme) - Chunk(%d,%d): dx = %.4f, dy = %.4f\n", cx, cy, w1.dx, w1.dy);
    assert(w1.dx == w2.dx && "ERREUR : Manque de determinisme sur dx !");
    assert(w1.dy == w2.dy && "ERREUR : Manque de determinisme sur dy !");
    printf("[PASS] Test 1: Intégration zyn_noise et déterminisme valides.\n");

    // Test 2 : Cohérence physique
    float speed = sqrtf(w1.dx * w1.dx + w1.dy * w1.dy);
    printf("Test 2 (Bornes) - Vitesse : %.2f km/h (Max: 90.00 km/h)\n", speed);
    assert(!isnan(w1.dx) && !isnan(w1.dy) && "ERREUR : Valeur NaN générée !");
    assert(speed <= 90.0001f && "ERREUR : Le vent dépasse la vitesse maximale !");
    printf("[PASS] Test 2: Bornes et typages OK.\n");

    printf("=== ALL TESTS PASSED FOR SUBTASK A ===\n");
    return 0;
}
