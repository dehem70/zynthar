/* =============================================================================
 *
 * ZYNTHAR v0.1
 *
 * Auteur : Dehem70
 * Date   : 30/05/2026
 *
 * test_wind_global : Validation du déterminisme et de la cohérence physique
 * du champ de vent global macro.
 * Génère automatiquement un rapport horodaté dans /reports/benchmarks/
 *
 * =============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include <zynthar.h>
#include "zyn_gen_wind_global.h"
#include "zyn_noise.h"

int main(void) {
    // 1. Récupération de la racine du projet Zynthar via l'environnement
    char *root_env = getenv("ZYNTHAR_ROOT");
    if (root_env == NULL) {
        fprintf(stderr, "[-] Erreur : La variable d'environnement ZYNTHAR_ROOT n'est pas définie.\n");
        fprintf(stderr, "💡 Exécutez : source ~/.bashrc\n");
        return EXIT_FAILURE;
    }

    // 2. Récupération des informations temporelles locales pour l'horodatage du rapport
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    if (tm_info == NULL) {
        fprintf(stderr, "[-] Erreur : Impossible de récupérer l'heure locale.\n");
        return EXIT_FAILURE;
    }

    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

    // 3. Construction du chemin absolu pour le rapport persistant
    char report_path[1024];
    snprintf(report_path, sizeof(report_path), "%s/reports/benchmarks/test_wind_global_%s.txt", root_env, timestamp);

    // 4. Ouverture du fichier de rapport en écriture
    FILE *report = fopen(report_path, "w");
    if (report == NULL) {
        fprintf(stderr, "[-] Erreur : Impossible de créer le fichier de rapport dans :\n    %s\n", report_path);
        fprintf(stderr, "💡 Vérifiez que l'arborescence /reports/benchmarks/ existe.\n");
        return EXIT_FAILURE;
    }

    // Macro unifiée écrivant simultanément sur la sortie standard et dans le fichier de log
    #define PRINT_BOTH(...) do { printf(__VA_ARGS__); fprintf(report, __VA_ARGS__); } while(0)

    PRINT_BOTH("=====================================================================\n");
    PRINT_BOTH("         ZYNTHAR : RAPPORT DE TEST DE COHÉRENCE (wind_global)        \n");
    PRINT_BOTH("=====================================================================\n");
    PRINT_BOTH(" Fichier généré : test_wind_global_%s.txt\n", timestamp);
    PRINT_BOTH(" Chemin absolu  : %s\n", report_path);
    PRINT_BOTH("=====================================================================\n\n");

    /* =========================================================================
     * INITALISATION DU MOTEUR MATHÉMATIQUE MUTUALISÉ
     * ========================================================================= */
    uint32_t seed_test = 424242U;
    PRINT_BOTH("[1/3] Initialisation de zyn_noise avec la graine %u...\n", seed_test);
    zyn_noise_init(seed_test);

    int32_t cx = 201;
    int32_t cy = 153;
    int32_t erreurs = 0;

    /* =========================================================================
     * PHASE 1 : VALIDATION DU DÉTERMINISME STRICT
     * ========================================================================= */
    PRINT_BOTH("[2/3] Vérification du déterminisme sur le Macro-Chunk (%d, %d)...\n", cx, cy);
    
    WindVector w1 = get_global_wind(cx, cy);
    WindVector w2 = get_global_wind(cx, cy);

    PRINT_BOTH("  -> Échantillon 1 : dx = %.4f, dy = %.4f\n", w1.dx, w1.dy);
    PRINT_BOTH("  -> Échantillon 2 : dx = %.4f, dy = %.4f\n", w2.dx, w2.dy);

    if (w1.dx != w2.dx || w1.dy != w2.dy) {
        PRINT_BOTH("  [ÉCHEC] Manque de déterminisme strict détecté sur les requêtes consécutives !\n");
        erreurs++;
    } else {
        PRINT_BOTH("  [SUCCÈS] Intégration zyn_noise et stabilité vectorielle validées.\n");
    }

    /* =========================================================================
     * PHASE 2 : CONFORMITÉ ET SÉCURITÉ PHYSIQUE DES BORNES
     * ========================================================================= */
    PRINT_BOTH("[3/3] Analyse de la plausibilité physique et des structures de données...\n");

    if (isnan(w1.dx) || isnan(w1.dy)) {
        PRINT_BOTH("  [ÉCHEC] Alerte critique : Valeur mathématique indéterminée (NaN) détectée.\n");
        erreurs++;
    }

    // Calcul de la vitesse scalaire via la norme Euclidienne du vecteur
    float vitesse_scal_kmh = sqrtf(w1.dx * w1.dx + w1.dy * w1.dy);
    PRINT_BOTH("  -> Vitesse scalaire calculée : %.2f km/h (Limite macro configurée : 90.00 km/h)\n", vitesse_scal_kmh);

    if (vitesse_scal_kmh > 90.0001f) {
        PRINT_BOTH("  [ÉCHEC] Violation des lois physiques : La vitesse dépasse le plafond de l'enveloppe macro.\n");
        erreurs++;
    }

    /* =========================================================================
     * CLÔTURE ET BILAN DU TEST UNITAIRE
     * ========================================================================= */
    PRINT_BOTH("\n=====================================================================\n");
    if (erreurs == 0) {
        PRINT_BOTH(" ✅ TOUS LES TESTS DE NON-RÉGRESSION SONT AU VERT POUR WIND_GLOBAL\n");
    } else {
        PRINT_BOTH(" ❌ ANOMALIE(S) DÉTECTÉE(S) : %d échec(s) enregistré(s).\n", erreurs);
    }
    PRINT_BOTH("=====================================================================\n");

    fclose(report);
    #undef PRINT_BOTH

    return (erreurs == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
