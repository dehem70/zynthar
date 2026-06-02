/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 01/06/2026
 *
 * zyn_test_framework  :
 * utilisation :
 *
 * =============================================================================*/
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sqlite3.h>

#include <zynthar.h>

#include "zyn_test_framework.h"


int32_t zyn_test_verify_continuity(const float* buffer, int32_t width_x, int32_t depth_z, 
                                   float seuil_rupture, uint32_t seed, const char* step_name) {
    int32_t compteur_ruptures = 0;
    float max_gradient_detecte = 0.0f;
    
    size_t total_cases = (size_t)width_x * (size_t)depth_z;
    uint8_t* gradient_pixels = (uint8_t*)calloc(total_cases, sizeof(uint8_t));
    if (gradient_pixels == NULL) {
        fprintf(stderr, "[ERR] Impossible d'allouer le buffer d'image de diagnostic.\n");
        return -1;
    }
    float min_val_detecte = buffer[0];
    float max_val_detecte = buffer[0];
    for (size_t i = 1; i < total_cases; i++) {
        if (buffer[i] < min_val_detecte) min_val_detecte = buffer[i];
        if (buffer[i] > max_val_detecte) max_val_detecte = buffer[i];
    }
    
    /* Pointeur de fichier pour consigner la liste exacte des coordonnées des brisures */
    FILE* log_file = NULL;
    char log_filename[256];

    /* On parcourt jusqu'à max-1 pour pouvoir regarder à Droite (x+1) et en Bas (z+1) */
    for (int32_t z = 0; z < depth_z - 1; z++) {
        for (int32_t x = 0; x < width_x - 1; x++) {
            size_t idx_centre = ZYN_INDEX(x, z, width_x);
            float val_centre = buffer[idx_centre];
            
            /* 1. Gradient Horizontal (Voisin de droite) */
            float val_droite = buffer[ZYN_INDEX(x + 1, z, width_x)];
            float diff_h = fabsf(val_centre - val_droite);

            /* 2. Gradient Vertical (Voisin du bas) */
            float val_bas = buffer[ZYN_INDEX(x, z + 1, width_x)];
            float diff_v = fabsf(val_centre - val_bas);

            /* On garde la plus forte pente locale pour les statistiques */
            float max_local_diff = (diff_h > diff_v) ? diff_h : diff_v;
            if (max_local_diff > max_gradient_detecte) {
                max_gradient_detecte = max_local_diff;
            }

            /* Validation du seuil */
            if (diff_h > seuil_rupture || diff_v > seuil_rupture) {
                compteur_ruptures++;
                gradient_pixels[idx_centre] = 255; /* On marque le pixel défaillant en blanc */

                /* Ouverture paresseuse du fichier log au premier pixel suspect trouvé */
                if (log_file == NULL) {
                    sprintf(log_filename, "audit_coordonnees_%s_seed_%u.log", step_name, seed);
                    log_file = fopen(log_filename, "w");
                    if (log_file != NULL) {
                        fprintf(log_file, "# RUN REPORT: %s | SEED: %u\n", step_name, seed);
                        fprintf(log_file, "# SEUIL TOLERANCE: %f\n", seuil_rupture);
                        fprintf(log_file, "# FORMAT: INDEX_MEMOIRE | COORDONNEE_X | COORDONNEE_Z | DELTA_DETECTE\n");
                    }
                }

                /* On consigne le coupable avec sa position et sa valeur de saut */
                if (log_file != NULL) {
                    fprintf(log_file, "%zu | %d | %d | %f\n", idx_centre, x, z, max_local_diff);
                }
            } else {
                float ratio = max_local_diff / (seuil_rupture > 0.0f ? seuil_rupture : 1.0f);
                if (ratio > 1.0f) ratio = 1.0f;
                gradient_pixels[idx_centre] = (uint8_t)(ratio * 120.0f);
            }
        }
    }

    /* Fermeture du fichier de log s'il a été ouvert */
    if (log_file != NULL) {
        fclose(log_file);
    }

 /* Rapport d'audit à l'écran mis à jour avec les points min/max */
    if (compteur_ruptures > 0) {
        printf("[AUDIT CRITIQUE] Pas: %s | Seed: %u | %d ruptures ! (Min: %.1f, Max: %.1f, Grad Max: %.1f) -> Log: %s\n", 
               step_name, seed, compteur_ruptures, min_val_detecte, max_val_detecte, max_gradient_detecte, log_filename);
        
        char img_filename[256];
        sprintf(img_filename, "audit_rupture_%s_seed_%u.png", step_name, seed);
        stbi_write_png(img_filename, width_x, depth_z, 1, gradient_pixels, 0);
    } else {
        printf("[AUDIT OK] Pas: %s | Seed: %u | Continuité validée (Min: %.1f, Max: %.1f, Grad Max: %.1f)\n", 
               step_name, seed, min_val_detecte, max_val_detecte, max_gradient_detecte);
    }
    free(gradient_pixels);
    return compteur_ruptures;
}
int32_t zyn_test_calibrate_framework(int32_t width_x, int32_t depth_z) {
    printf("[CALIBRATION] Lancement de l'auto-test de validation du framework...\n");

    size_t total_cases = (size_t)width_x * (size_t)depth_z;
    float* buffer_test = (float*)malloc(total_cases * sizeof(float));
    if (buffer_test == NULL) return 0;

    /* On fixe un seuil de rupture strict à 50.0 pour calibrer nos deux simulations */
    float seuil_rupture_test = 50.0f; 

    // -------------------------------------------------------------------------
    // SIMULATION A : CARTE SANS DISCONTINUITÉ (Pente linéaire lisse)
    // -------------------------------------------------------------------------
    for (int32_t z = 0; z < depth_z; z++) {
        for (int32_t x = 0; x < width_x; x++) {
            size_t idx = ZYN_INDEX(x, z, width_x);
            /* Chaque pixel augmente doucement de 1.0f, la différence max avec un voisin sera de 1.414f */
            buffer_test[idx] = (float)x + (float)z; 
        }
    }

    /* On s'attend impérativement à obtenir 0 rupture */
    int32_t ruptures_carte_ok = zyn_test_verify_continuity(buffer_test, width_x, depth_z, seuil_rupture_test, 0, "CALIB_PURE_OK");

    // -------------------------------------------------------------------------
    // SIMULATION B : CARTE CORROMPUE (Sabotage volontaire d'un pixel)
    // -------------------------------------------------------------------------
    /* On injecte une faille béante : la colonne du milieu s'effondre brutalement à -1000.0f */
    int32_t x_milieu = width_x / 2;
    int32_t z_milieu = depth_z /2;
    int32_t pixels_sabotes = 1;

    size_t idx_milieu = ZYN_INDEX(x_milieu, z_milieu, width_x);
    buffer_test[idx_milieu] += 100.0f; 

    /* On s'attend à obtenir un nombre de ruptures égal (ou proche) au nombre de pixels sabotés */
    int32_t ruptures_carte_nok = zyn_test_verify_continuity(buffer_test, width_x, depth_z, seuil_rupture_test, 0, "CALIB_MUTATION_NOK");

    free(buffer_test);

    // -------------------------------------------------------------------------
    // VERDICT DE LA COMPÉTENCE DU THERMOMÈTRE
    // -------------------------------------------------------------------------
    int32_t framework_valide = 1;

    if (ruptures_carte_ok != 0) {
        printf("[ERREUR FRAMEWORK] Faux Positif détecté ! L'outil voit des problèmes là où il n'y en a pas.\n");
        framework_valide = 0;
    }
    if (ruptures_carte_nok == 0) {
        printf("[ERREUR FRAMEWORK] Faux Négatif détecté ! L'outil est aveugle aux fractures majeures.\n");
        framework_valide = 0;
    }

    if (framework_valide) {
        printf("[CALIBRATION REUSSIE] L'instrument de mesure est validé (OK=0, NOK=%d/%d). Sécurité confirmée.\n\n", 
               ruptures_carte_nok/4, pixels_sabotes);
    } else {
        printf("[CALIBRATION CRITIQUE FAILED] L'outil de contrôle est défaillant. Suspension immédiate.\n\n");
    }

    return framework_valide;
}
