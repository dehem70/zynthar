/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 13/06/2026
 *
 * zyn_hecate  :
 * utilisation :
 *
 * =============================================================================*/
  

#include <zynthar.h>
// zyn_hecate.c
#include "zyn_hecate.h"
#include "zyn_hecate_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

// Variable globale pour contrôler la boucle principale
static volatile int g_hecate_running = 1;

// Gestionnaire de signaux pour couper le programme proprement (Ctrl+C, kill)
void zyn_hecate_handle_signal(int sig) {
    (void)sig; // Évite le warning "unused variable"
    printf("\n[💾 HÉCATE] Signal de fermeture reçu. Extinction en cours...\n");
    g_hecate_running = 0;
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv; // Pour l'instant on ne gère pas d'arguments
    
    printf("[💾 HÉCATE] Démarrage de la gardienne de l'arbre...\n");

    // 1. Enregistrement des signaux système pour le clean automatique
    signal(SIGINT, zyn_hecate_handle_signal);
    signal(SIGTERM, zyn_hecate_handle_signal);

    // 2. Initialisation et allocation de la Couche 1 en SHM
    if (zyn_hecate_init_layer1() != 0) {
        fprintf(stderr, "[❌ HÉCATE] Échec critique lors du boot de la Couche 1.\n");
        return EXIT_FAILURE;
    }

    printf("[✅ HÉCATE] Initialisation réussie couche 1.\n");
    
    char db_path[512];
    get_db_path(db_path, ZYN_DB_WORLD);
    
    uint32_t complexes_detectes = zyn_hecate_couche1_from_db(db_path);
    
    if (complexes_detectes < 0) {
        zyn_hecate_clean_shm();
        return EXIT_FAILURE;
    }
    
    printf("[🚀 HÉCATE] Couche 1 peuplée. Nécessité d'avoir une couche 2 de %u cases\n",complexes_detectes);


    // 3. Allocation immédiate de la Couche 2 à la taille parfaite !
    if (zyn_hecate_init_layer2((uint32_t)complexes_detectes) != 0) {
        zyn_hecate_clean_shm();
        return EXIT_FAILURE;
    }

    printf("[✅ HÉCATE] Initialisation réussie couche 2.\n");
    
    complexes_detectes = zyn_hecate_couche2_from_db(db_path);
    
    if (complexes_detectes < 0) {
        zyn_hecate_clean_shm();
        return EXIT_FAILURE;
    }
    
    printf("[🚀 HÉCATE] Couche 2 peuplée. \n");


    printf("[🚀 HÉCATE] en mode écoute. \n");
    
    while (g_hecate_running) {
        // Ici, Hécate écoutera sa file de tickets ou fera sa maintenance
        sleep(1); 
    }

    // 4. Nettoyage de la SHM avant de quitter
    zyn_hecate_clean_shm();
    
    printf("[👋 HÉCATE] Processus arrêté proprement. À bientôt.\n");
    return EXIT_SUCCESS;
}
