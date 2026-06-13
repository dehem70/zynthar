/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 13/06/2026
 *
 * zyn_hecate_utils  :
 * utilisation :
 *
 * =============================================================================*/
  
#include "zyn_hecate_utils.h"
#include "zynthar.h" // Ta configuration globale du monde

#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>


// Définition des variables globales
HecateRegion* g_hecate_regions = NULL;
size_t g_hecate_l1_shm_size = 0;

HecateLevel2Node* g_hecate_layer2_pool = NULL;
size_t g_hecate_l2_shm_size = 0;


int zyn_hecate_init_layer1(void) {
    // Calcul de la taille : 32 régions * ~1.5 Mo
    uint32_t total_regions = ZYN_WORLD_REGION_X * ZYN_WORLD_REGION_Z;
    g_hecate_l1_shm_size = total_regions * sizeof(HecateRegion);
    
    printf("[💾 HÉCATE] Allocation SHM par Régions... Taille totale : %.2f Mo\n", 
           (double)g_hecate_l1_shm_size / (1024.0 * 1024.0));

    // Ouverture du segment SHM
    int shm_fd = shm_open(HECATE_SHM_NAME_L1, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("[❌ HÉCATE] Erreur shm_open");
        return -1;
    }

    // Dimensionnement strict
    if (ftruncate(shm_fd, g_hecate_l1_shm_size) == -1) {
        perror("[❌ HÉCATE] Erreur ftruncate");
        close(shm_fd);
        return -1;
    }

    // Projection mémoire
    g_hecate_regions = (HecateRegion*)mmap(
        NULL, 
        g_hecate_l1_shm_size, 
        PROT_READ | PROT_WRITE, 
        MAP_SHARED, 
        shm_fd, 
        0
    );

    close(shm_fd);

    if (g_hecate_regions == MAP_FAILED) {
        perror("[❌ HÉCATE] Erreur mmap");
        g_hecate_regions = NULL;
        return -1;
    }

    printf("[✅ HÉCATE] SHM Couche 1 (%u régions) projetée à l'adresse %p\n", 
           total_regions, (void*)g_hecate_regions);
    return 0;
}


// Fonction utilitaire pour générer le chemin complet de la base de données
void get_db_path(char *dest, const char *db_name) {
    char *root_env = getenv("ZYNTHAR_ROOT");
    if (root_env != NULL) {
        sprintf(dest, "%s/%s%s", root_env, ZYN_DB_EMPLACEMENT, db_name);
    } else {
        sprintf(dest, "./%s%s", ZYN_DB_EMPLACEMENT, db_name);
    }
}

uint32_t  zyn_hecate_couche1_from_db(const char* db_path) {
    printf("[💾 HÉCATE] Ouverture de la base de données reliefs : %s\n", db_path);
    
    sqlite3* db = NULL;
    // Mode lecture seule pour maximiser la vitesse du streaming
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        fprintf(stderr, "[❌ HÉCATE] Impossible d'ouvrir la DB : %s\n", sqlite3_errmsg(db));
        return -1;
    }

    // Extraction de l'ID binaire et du BLOB de métadonnées
    const char* query = "SELECT id, data FROM macro_chunks;";
    sqlite3_stmt* stmt = NULL;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[❌ HÉCATE] Échec de préparation de la requête : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    uint32_t total_air = 0;
    uint32_t total_roche = 0;
    uint32_t total_complex = 0;
    uint32_t total_eau = 0;

    const int16_t CHUNK_HEIGHT_DM = ZYN_MACRO_CHUNK_DIM_M *10 ;

    printf("[💾 HÉCATE] Analyse géologique et peuplement des régions en RAM...\n");

    // Boucle de streaming à haute vitesse
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        // 1. Récupération et parsing de l'ID via ton union zynthar
        Id macro_id;
        macro_id.id = (uint32_t)sqlite3_column_int64(stmt, 0);

        // 2. Calcul des index de destination dans notre SHM
        uint32_t region_idx  = macro_id.rx + (macro_id.rz * ZYN_WORLD_REGION_X);
        uint32_t colonne_idx = macro_id.x + (macro_id.z * ZYN_WORLD_MACRO_DIM);

        // 3. Récupération du BLOB de relief
        const void* blob_ptr = sqlite3_column_blob(stmt, 1);
        int blob_size = sqlite3_column_bytes(stmt, 1);

        if (blob_size != sizeof(MacroChunk_db)) {
            // Sécurité si la taille du BLOB en base est corrompue
            continue;
        }

        const MacroChunk_db* meta = (const MacroChunk_db*)blob_ptr;

        // 4. Calcul de l'altitude minimale des coins pour détecter la roche profonde
        int16_t min_h = meta->elevation_coin_nw;
        int16_t max_h=meta->elevation_max_dm;
        if (meta->elevation_coin_ne < min_h) min_h = meta->elevation_coin_ne;
        if (meta->elevation_coin_se < min_h) min_h = meta->elevation_coin_se;
        if (meta->elevation_coin_sw < min_h) min_h = meta->elevation_coin_sw;
        if (meta->elevation_coin_ne > max_h) max_h = meta->elevation_coin_ne;
        if (meta->elevation_coin_se > max_h) max_h = meta->elevation_coin_se;
        if (meta->elevation_coin_sw > max_h) max_h = meta->elevation_coin_sw;
        
        if (max_h<min_h) max_h=min_h;
        if (min_h>max_h) min_h=max_h;
        
        //on prend une marge de 50 m en plus et 100 m en moins pour relief locaux et premiere couche avant la roche 
        min_h-=1000;
        max_h+=500;
        uint32_t y_index=0;
        static int32_t y_min = ZYN_WORLD_Y_MIN / ZYN_MACRO_CHUNK_DIM_M; // Donne -2
        static int32_t y_max = ZYN_WORLD_Y_MAX / ZYN_MACRO_CHUNK_DIM_M; // Donne +4 (exclu)
        // 🎯 5. Analyse verticale pour les 6 étages (Y = 0 à 5)
        for (int32_t y = y_min; y < y_max; y++) {
            int16_t chunk_bottom_dm = (int16_t)(y * CHUNK_HEIGHT_DM);
            int16_t chunk_top_dm    = (int16_t)((y + 1) * CHUNK_HEIGHT_DM);
            
            if (max_h <= chunk_bottom_dm) {
                // Tout le relief est en dessous du plancher de cet étage -> Air pur
                if (chunk_bottom_dm >=0) {
                    g_hecate_regions[region_idx].colonnes[colonne_idx].etages[y_index].level2_index = HECATE_STATE_AIR;
                    total_air++;
                } else if (chunk_top_dm <= 0) {
                    g_hecate_regions[region_idx].colonnes[colonne_idx].etages[y_index].level2_index = HECATE_STATE_EAU;
                    total_eau++;
                } else {
                // Le chunk traverse la surface de l'eau (le niveau 0 est à l'intérieur) -> COMPLEXE !
                    g_hecate_regions[region_idx].colonnes[colonne_idx].etages[y_index].level2_index = HECATE_STATE_MIXTE;
                    total_complex++;
                }   
            } 
            else if (min_h >= chunk_top_dm) {
                // Tous les coins sont au-dessus du plafond de cet étage -> Roche pure
                g_hecate_regions[region_idx].colonnes[colonne_idx].etages[y_index].level2_index = HECATE_STATE_ROCHE;
                total_roche++;
            } 
            else {
                // Le relief traverse l'étage ou la surface s'y trouve -> Mixte/Complexe
                g_hecate_regions[region_idx].colonnes[colonne_idx].etages[y_index].level2_index = HECATE_STATE_MIXTE;
                total_complex++;
            }
            y_index++;
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    printf("[✅ HÉCATE] Traitement terminé avec succès.\n");
    printf("   -> Macro-Chunks [AIR]      : %llu\n", (unsigned long long)total_air);
    printf("   -> Macro-Chunks [ROCHE]    : %llu\n", (unsigned long long)total_roche);
    printf("   -> Macro-Chunks [EAU]      : %llu\n", (unsigned long long)total_eau);
    printf("   -> Macro-Chunks [COMPLEXE] : %llu\n", (unsigned long long)total_complex);
    printf("   -> Total des nœuds indexés : %llu (doit être égal à 6 * 2097152 = 12582912)\n", 
           (unsigned long long)(total_air + total_roche + total_complex+total_eau));

    return total_complex;
}

/**
 * @brief Package l'état/matière et l'index Couche 3 dans un entier 32 bits.
 * * @param material ID de la matière (0 à 254) ou HECATE_STATE_MIXTE (0xFF)
 * @param index    Index de la feuille Couche 3 (0 à 16777215)
 * @return uint32_t Le nœud de Couche 2 packagé
 */
static inline uint32_t hecate_l2_pack(uint8_t material, uint32_t index) {
    // 1. On décale la matière de 24 bits vers la gauche pour la mettre tout en haut
    uint32_t packed_mat = (uint32_t)material << 24;
    
    // 2. On applique le masque sur l'index pour s'assurer qu'il ne dépasse pas 24 bits
    uint32_t packed_idx = index & HECATE_L2_INDEX_MASK;
    
    // 3. On fusionne les deux avec un OU binaire (|)
    return (packed_mat | packed_idx);
}

uint32_t zyn_hecate_couche2_from_db(const char* db_path) {
    printf("[💾 HÉCATE] Début du peuplement de la Couche 2 (Granulométrie fine)...\n");

    sqlite3* db = NULL;
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        fprintf(stderr, "[❌ HÉCATE] Impossible d'ouvrir la DB pour la Couche 2 : %s\n", sqlite3_errmsg(db));
        return -1;
    }

    // On reprend la même requête ordonnée pour suivre exactement le même fil d'Ariane
    const char* query = "SELECT id, data FROM macro_chunks;";
    sqlite3_stmt* stmt = NULL;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[❌ HÉCATE] Échec préparation Couche 2 : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    uint32_t layer2_pool_index = 0; // Pointeur d'écriture linéaire dans le Pool de Couche 2
    uint64_t total_c3_leaves = 0;   // Notre compteur magique pour la future Couche 3 !

    const int16_t TRANCHE_HEIGHT_DM = (ZYN_MICRO_CHUNK_DIM_VOX*ZYN_VOXEL_TO_M*10);
    
    printf("[✅ HÉCATE] Analyse de couches de hauteurs %u dm.\n",TRANCHE_HEIGHT_DM);

    uint32_t cmpt_macro=0;
    uint32_t cmpt_macro_ana=0;
    uint32_t cmpt_couche=0;

    static int32_t y_min = ZYN_WORLD_Y_MIN / ZYN_MACRO_CHUNK_DIM_M; // Donne -2
    static int32_t y_max = ZYN_WORLD_Y_MAX / ZYN_MACRO_CHUNK_DIM_M; // Donne +4 (exclu)
    static uint32_t tmax=(ZYN_MACRO_CHUNK_DIM_M/(ZYN_MICRO_CHUNK_DIM_VOX*ZYN_VOXEL_TO_M));
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Id macro_id;
        macro_id.id = (uint32_t)sqlite3_column_int64(stmt, 0);
        
        uint32_t region_idx  = macro_id.rx + (macro_id.rz * ZYN_WORLD_REGION_X);
        uint32_t colonne_idx = macro_id.x + (macro_id.z * ZYN_WORLD_MACRO_DIM);


        // 🎯 5. Analyse verticale pour les 6 étages (Y = 0 à 5)
        for (int32_t y = y_min; y < y_max; y++) {
            cmpt_macro++;
            uint32_t y_index = (uint32_t)(y - y_min);
            
            // Si l'étage n'est pas complexe, on l'ignore (il est déjà Shunté en Air/Roche/Eau)
            if (g_hecate_regions[region_idx].colonnes[colonne_idx].etages[y_index].level2_index != HECATE_MAT_MIXTE) {
                continue;
            }

            // 🎯 On a trouvé un bloc complexe !
            cmpt_macro_ana++;
            // 1. On remplace le tag HECATE_STATE_MIXTE  de la Couche 1 par l'index de sa première tranche dans le Pool L2
            g_hecate_regions[region_idx].colonnes[colonne_idx].etages[y_index].level2_index = layer2_pool_index;

            // 2. On récupère le BLOB de relief pour analyser les 20 sous-tranches
            const void* blob_ptr = sqlite3_column_blob(stmt, 1);
            const MacroChunk_db* meta = (const MacroChunk_db*)blob_ptr;

            // On recalcule le min_h et max_h spécifiques avec tes marges de sécurité
            int16_t min_h = meta->elevation_coin_nw;
            int16_t max_h = meta->elevation_max_dm;
            if (meta->elevation_coin_ne < min_h) min_h = meta->elevation_coin_ne;
            if (meta->elevation_coin_se < min_h) min_h = meta->elevation_coin_se;
            if (meta->elevation_coin_sw < min_h) min_h = meta->elevation_coin_sw;
            if (meta->elevation_coin_ne > max_h) max_h = meta->elevation_coin_ne;
            if (meta->elevation_coin_se > max_h) max_h = meta->elevation_coin_se;
            if (meta->elevation_coin_sw > max_h) max_h = meta->elevation_coin_sw;
            if (max_h < min_h) max_h = min_h;
            if (min_h > max_h) min_h = max_h;
            
            min_h -= 1000; // Marge basse (Terre/Sable/Creux)
            max_h += 500;  // Marge haute (Bosses)

            // Calcul de l'altitude de départ en Y pour cet étage précis
            int32_t y_macro = (ZYN_WORLD_Y_MIN / ZYN_MACRO_CHUNK_DIM_M) + y_index;
            int16_t macro_bottom_dm = (int16_t)(y_macro * ZYN_MACRO_CHUNK_DIM_M *10);


            // 3. Boucle de granularité fine : On analyse les tmax micro-chunks verticaux
            for (uint32_t t = 0; t < tmax; t++) {
                cmpt_couche++;
                int16_t tranche_bottom_dm = (int16_t)(macro_bottom_dm + (t * TRANCHE_HEIGHT_DM));
                int16_t tranche_top_dm    = (int16_t)(tranche_bottom_dm + TRANCHE_HEIGHT_DM);

                uint8_t packed_node = 0;

                if (max_h <= tranche_bottom_dm) {
                    // Tranche purement au-dessus du relief solide
                    if (tranche_bottom_dm >= 0) {
                        packed_node = HECATE_MAT_AIR;
                    } else if (tranche_top_dm <= 0) {
                        packed_node = HECATE_MAT_EAU;
                    } else {
                        // Interface Air / Eau au niveau 0
                        packed_node = HECATE_MAT_MIXTE;
                        total_c3_leaves++;
                    }
                } 
                else if (min_h >= tranche_top_dm) {
                    // Tranche purement sous la roche profonde
                    packed_node = HECATE_MAT_ROCHE;
                } 
                else {

                    // Elle est hétérogène, elle aura besoin de la Couche 3 (Feuille de 4 Ko)
                    packed_node = HECATE_MAT_MIXTE;
                    total_c3_leaves++;
                }

                // Écriture physique dans notre pool SHM
                g_hecate_layer2_pool[layer2_pool_index + t].matiere = packed_node;
            }

            // On fait avancer le pointeur du pool de tmax cases pour le prochain bloc complexe
            layer2_pool_index += tmax;
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    printf("[✅ HÉCATE] Couche 2 entièrement peuplée.\n");
    printf("   -> Analyse de %u macrochunks dont %u complexes (%u couches)\n",cmpt_macro,cmpt_macro_ana,cmpt_couche);
    printf("   -> %u couches par macrochunks\n",tmax);
    printf("   -> Tranches L2 écrites     : %u\n", layer2_pool_index);
    printf("   -> Tranches complexes      : %llu\n", (unsigned long long)total_c3_leaves);

    return (int32_t)total_c3_leaves;
}

// --- 🎯 INITIALISATION COUCHE 2 (Taille sur-mesure !) ---
int zyn_hecate_init_layer2(uint32_t exact_complex_count) {
    // Chaque bloc complexe contient 16 tranches de 4 octets (64 octets par bloc)
    uint64_t total_l2_nodes = (uint64_t)exact_complex_count * 16;
    g_hecate_l2_shm_size = total_l2_nodes * sizeof(HecateLevel2Node);

    printf("[💾 HÉCATE] Allocation SHM Couche 2 pour %u blocs complexes... Taille : %.2f Mo\n", 
           exact_complex_count, (double)g_hecate_l2_shm_size / (1024.0 * 1024.0));

    int shm_fd = shm_open(HECATE_SHM_NAME_L2, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("[❌ HÉCATE] Erreur shm_open Couche 2");
        return -1;
    }

    if (ftruncate(shm_fd, g_hecate_l2_shm_size) == -1) {
        perror("[❌ HÉCATE] Erreur ftruncate Couche 2");
        close(shm_fd);
        return -1;
    }

    g_hecate_layer2_pool = (HecateLevel2Node*)mmap(NULL, g_hecate_l2_shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);

    if (g_hecate_layer2_pool == MAP_FAILED) {
        perror("[❌ HÉCATE] Erreur mmap Couche 2");
        g_hecate_layer2_pool = NULL;
        return -1;
    }

    printf("[✅ HÉCATE] SHM Couche 2 projetée avec succès à l'adresse %p\n", (void*)g_hecate_layer2_pool);
    return 0;
}

void zyn_hecate_clean_shm(void) {
    if (g_hecate_regions && g_hecate_regions != MAP_FAILED) munmap(g_hecate_regions, g_hecate_l1_shm_size);
    shm_unlink(HECATE_SHM_NAME_L1);

    if (g_hecate_layer2_pool && g_hecate_layer2_pool != MAP_FAILED) munmap(g_hecate_layer2_pool, g_hecate_l2_shm_size);
    shm_unlink(HECATE_SHM_NAME_L2);
 
    printf("[💾 HÉCATE] Nettoyage complet des SHM effectué.\n");
}

