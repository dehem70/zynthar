/* =============================================================================
 *
 * ZYNTHAR v0.1
 *
 * Auteur : Dehem70
 * Date   : 27/05/2026
 *
 * zyn_db_cli : Programme de gestion et maintenance des bases de données SQLite3
 * Utilisation : zyn_db_cli <commande> [options]
 *
 * Commandes :
 * initw                                 => Initialisation de la base zyn-world.db
 * add-macro <x> <z> <biome> <t> <h> <y> => Ajoute ou met à jour un macro-chunk
 * export-csv <fichier.csv>              => Exporte la base dans un fichier CSV
 * import-csv <fichier.csv>              => Importe un fichier CSV (écrase l'existant)
 * populate-random                       => Remplit la grille globale de données pseudo-aléatoires
 * stress-test                           => Exécute 10 000 requêtes de lecture aléatoires
 * info                                  => Affiche la structure et les métriques de la base
 *
 * =============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>

// En-têtes requis sous Linux/POSIX pour manipuler 'struct stat' et 'mkdir'
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Inclusion globale propre de la source de vérité géométrique
#include <zynthar.h>

// Fonction d'aide pour afficher l'usage du CLI
void print_usage(const char *prog_name) {
    printf("Usage: %s <commande> [options]\n", prog_name);
    printf("Commandes disponibles :\n");
    printf("  initw                                       Crée la base de données et la table macro_chunks\n");
    printf("  add-macro <x> <z> <biome> <temp> <hum> <y>  Ajoute ou met à jour un macro-chunk (Y en mètres)\n");
    printf("  export-csv <fichier.csv>                    Exporte la table macro_chunks au format CSV\n");
    printf("  import-csv <fichier.csv>                    Importe un fichier CSV dans macro_chunks (écrase l'existant)\n");
    printf("  populate-random                             Remplit la grille du monde de données aléatoires\n");
    printf("  stress-test                                 Exécute 10 000 requêtes de lecture aléatoires\n");
    printf("  info                                        Affiche la structure et le nombre de lignes de la base\n");
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

// Assure que le dossier des bases de données existe au bon endroit
int ensure_db_dir() {
    char dir_path[512];
    char *root_env = getenv("ZYNTHAR_ROOT");

    if (root_env != NULL) {
        sprintf(dir_path, "%s/%s", root_env, ZYN_DB_EMPLACEMENT);
    } else {
        sprintf(dir_path, "./%s", ZYN_DB_EMPLACEMENT);
    }

    struct stat st = {0};
    if (stat(dir_path, &st) == -1) {
        #ifdef _WIN32
        if (mkdir(dir_path) != 0) {
        #else
        if (mkdir(dir_path, 0755) != 0) {
        #endif
            fprintf(stderr, "[-] Impossible de créer le dossier des bases de données : %s\n", dir_path);
            return -1;
        }
        printf("[+] Dossier des bases de données créé : %s\n", dir_path);
    }
    return 0;
}

// Commande : EXPORT-CSV
int cmd_export_csv(const char *filename) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    char db_path[512];
    get_db_path(db_path, ZYN_DB_WORLD);

    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Impossible d'ouvrir la base : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    FILE *csv_file = fopen(filename, "w");
    if (csv_file == NULL) {
        fprintf(stderr, "[-] Erreur : Impossible de créer le fichier %s\n", filename);
        sqlite3_close(db);
        return 1;
    }

    // Extraction au format physique lisible par l'humain
    fprintf(csv_file, "chunk_x,chunk_z,biome_type,temperature,humidity,max_elevation_m\n");

    const char *sql_select = "SELECT chunk_x, chunk_z, biome_type, temperature, humidity, max_elevation FROM macro_chunks;";
    rc = sqlite3_prepare_v2(db, sql_select, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Erreur lors de la préparation de la lecture : %s\n", sqlite3_errmsg(db));
        fclose(csv_file);
        sqlite3_close(db);
        return 1;
    }

    int row_count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int x = sqlite3_column_int(stmt, 0);
        int z = sqlite3_column_int(stmt, 1);
        int biome = sqlite3_column_int(stmt, 2);
        uint8_t temp_raw = (uint8_t)sqlite3_column_int(stmt, 3);
        uint8_t hum_raw = (uint8_t)sqlite3_column_int(stmt, 4);
        int16_t elev_dm = (int16_t)sqlite3_column_int(stmt, 5);

        // Re-conversion immédiate des unités brutes en unités physiques réelles pour le CSV
        float temp = RAW_TO_FLOAT(temp_raw);
        float hum = RAW_TO_FLOAT(hum_raw);
        float elev = DM_TO_M(elev_dm);

        fprintf(csv_file, "%d,%d,%d,%.4f,%.4f,%.1f\n", x, z, biome, temp, hum, elev);
        row_count++;
    }

    sqlite3_finalize(stmt);
    fclose(csv_file);
    sqlite3_close(db);

    printf("[+] Export terminé avec succès. %d macro-chunks exportés dans '%s'.\n", row_count, filename);
    return 0;
}

// Commande : IMPORT-CSV
int cmd_import_csv(const char *filename) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    char db_path[512];
    get_db_path(db_path, ZYN_DB_WORLD);

    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Impossible d'ouvrir la base : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    const char *sql_count = "SELECT COUNT(*) FROM macro_chunks;";
    int count = 0;
    rc = sqlite3_prepare_v2(db, sql_count, -1, &stmt, 0);
    if (rc == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (count > 0) {
        printf("[!] Attention : La table macro_chunks contient déjà %d enregistrement(s).\n", count);
        printf("[?] Voulez-vous TOUT effacer et importer ce CSV ? (y/N) : ");
        fflush(stdout);
        
        char choice = getchar();
        if (choice != '\n' && choice != EOF) {
            while (getchar() != '\n');
        }

        if (choice != 'y' && choice != 'Y') {
            printf("[-] Opération annulée par l'utilisateur.\n");
            sqlite3_close(db);
            return 0;
        }

        char *err_msg = 0;
        rc = sqlite3_exec(db, "DELETE FROM macro_chunks;", 0, 0, &err_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[-] Erreur lors du nettoyage de la table : %s\n", err_msg);
            sqlite3_free(err_msg);
            sqlite3_close(db);
            return 1;
        }
        printf("[+] Anciennes données effacées.\n");
    }

    FILE *csv_file = fopen(filename, "r");
    if (csv_file == NULL) {
        fprintf(stderr, "[-] Erreur : Impossible d'ouvrir le fichier %s\n", filename);
        sqlite3_close(db);
        return 1;
    }

    const char *sql_insert = 
        "INSERT INTO macro_chunks (chunk_x, chunk_z, biome_type, temperature, humidity, max_elevation) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    
    rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Erreur préparation insertion : %s\n", sqlite3_errmsg(db));
        fclose(csv_file);
        sqlite3_close(db);
        return 1;
    }

    sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, 0);

    char line[512];
    int line_count = 0;
    int imported_count = 0;

    while (fgets(line, sizeof(line), csv_file)) {
        line_count++;
        
        if (line_count == 1 && strstr(line, "chunk_x") != NULL) {
            continue;
        }

        int x, z, biome;
        double temp_input, hum_input, elev_input;

        if (sscanf(line, "%d,%d,%d,%lf,%lf,%lf", &x, &z, &biome, &temp_input, &hum_input, &elev_input) == 6) {
            // Sérialisation et packing immédiat vers notre structure d'octets légere
            uint8_t temp_raw = FLOAT_TO_RAW(temp_input);
            uint8_t hum_raw = FLOAT_TO_RAW(hum_input);
            int16_t elev_dm = M_TO_DM(elev_input);

            sqlite3_reset(stmt);
            sqlite3_bind_int(stmt, 1, x);
            sqlite3_bind_int(stmt, 2, z);
            sqlite3_bind_int(stmt, 3, biome);
            sqlite3_bind_int(stmt, 4, temp_raw);
            sqlite3_bind_int(stmt, 5, hum_raw);
            sqlite3_bind_int(stmt, 6, elev_dm);

            if (sqlite3_step(stmt) == SQLITE_DONE) {
                imported_count++;
            }
        } else {
            fprintf(stderr, "[*] Alerte : Ligne %d mal formatée ignorée.\n", line_count);
        }
    }

    sqlite3_exec(db, "COMMIT;", 0, 0, 0);
    sqlite3_finalize(stmt);
    fclose(csv_file);
    sqlite3_close(db);

    printf("[+] Importation terminée. %d macro-chunks importés avec succès.\n", imported_count);
    return 0;
}

// Commande : POPULATE-RANDOM
int cmd_populate_random() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    char db_path[512];
    get_db_path(db_path, ZYN_DB_WORLD);

    // Calcul géométrique précis fondé sur le nouveau macro-chunk à 512 m [cite: 52]
    int max_chunks_x = ZYN_WORLD_X_MAX / ZYN_MACRO_CHUNK_DIM_M;
    int max_chunks_z = ZYN_WORLD_Z_MAX / ZYN_MACRO_CHUNK_DIM_M;
    long long total_chunks = (long long)max_chunks_x * max_chunks_z;

    printf("[*] Configuration globale de l'univers :\n");
    printf("    -> Dimensions de la grille de Macro-Chunks : %d x %d\n", max_chunks_x, max_chunks_z);
    printf("    -> Total d'éléments à injecter          : %lld macro-chunks\n", total_chunks);

    printf("[?] Confirmez-vous la génération aléatoire de la base (effacement complet) ? (y/N) : ");
    fflush(stdout);
    char choice = getchar();
    if (choice != '\n' && choice != EOF) { while (getchar() != '\n'); }
    if (choice != 'y' && choice != 'Y') {
        printf("[-] Opération annulée.\n");
        return 0;
    }

    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Impossible d'ouvrir la base : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    clock_t start_time = clock();

    // Configuration optimale des PRAGMAs de performance en écriture 
    sqlite3_exec(db, "PRAGMA synchronous = OFF;", 0, 0, 0);
    sqlite3_exec(db, "PRAGMA journal_mode = MEMORY;", 0, 0, 0);
    sqlite3_exec(db, "PRAGMA cache_size = -80000;", 0, 0, 0); // 80 Mo de cache RAM dédié
    sqlite3_exec(db, "PRAGMA locking_mode = EXCLUSIVE;", 0, 0, 0);

    sqlite3_exec(db, "DELETE FROM macro_chunks;", 0, 0, 0);

    const char *sql_insert = 
        "INSERT INTO macro_chunks (chunk_x, chunk_z, biome_type, temperature, humidity, max_elevation) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Erreur préparation : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    // Registre Xorshift rapide pour le stress-test et le remplissage
    unsigned int x32 = (unsigned int)time(NULL); 
    int16_t min_dm = ZYN_WORLD_Y_MIN * 10;
    int16_t max_dm = ZYN_WORLD_Y_MAX * 10;
    int32_t range_dm = max_dm - min_dm;

    printf("[+] Injection massive en cours...\n");
    sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, 0);

    long long inserted = 0;
    for (int x = 0; x < max_chunks_x; x++) {
        sqlite3_bind_int(stmt, 1, x); 
        
        for (int z = 0; z < max_chunks_z; z++) {
            // Algorithme PRNG Xorshift 32 bits ultra-léger
            x32 ^= x32 << 13;
            x32 ^= x32 >> 17;
            x32 ^= x32 << 5;

            // Attribution brute respectant les bornes de notre structure
            uint8_t biome = (x32 % 12) + 1; // Évite BIOME_INCONNU (0)
            uint8_t temp_raw = x32 % 256;
            uint8_t hum_raw = (x32 >> 8) % 256;
            int16_t elev_dm = min_dm + (int16_t)(x32 % range_dm);

            sqlite3_bind_int(stmt, 2, z);
            sqlite3_bind_int(stmt, 3, biome);
            sqlite3_bind_int(stmt, 4, temp_raw);
            sqlite3_bind_int(stmt, 5, hum_raw);
            sqlite3_bind_int(stmt, 6, elev_dm);

            sqlite3_step(stmt);
            sqlite3_reset(stmt);
            
            inserted++;
        }
        if (x % 100 == 0) {
            printf("\r    Progression de l'injection : %.1f%%", ((double)inserted / total_chunks) * 100.0);
            fflush(stdout);
        }
    }

    sqlite3_exec(db, "COMMIT;", 0, 0, 0);
    sqlite3_finalize(stmt);
    sqlite3_exec(db, "PRAGMA locking_mode = NORMAL;", 0, 0, 0);
    sqlite3_close(db);

    clock_t end_time = clock();
    double time_taken = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

    printf("\r[+] Terminé ! %lld macro-chunks générés en %.3f secondes.\n", inserted, time_taken);
    return 0;
}

// Commande : STRESS-TEST
int cmd_stress_test() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    char db_path[512];
    get_db_path(db_path, ZYN_DB_WORLD);

    int max_chunks_x = ZYN_WORLD_X_MAX / ZYN_MACRO_CHUNK_DIM_M;
    int max_chunks_z = ZYN_WORLD_Z_MAX / ZYN_MACRO_CHUNK_DIM_M;
    int num_queries = 10000;

    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Erreur ouverture : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    sqlite3_exec(db, "PRAGMA cache_size = -80000;", 0, 0, 0); 
    sqlite3_exec(db, "PRAGMA read_uncommitted = TRUE;", 0, 0, 0);

    const char *sql_query = "SELECT biome_type, max_elevation FROM macro_chunks WHERE chunk_x = ? AND chunk_z = ?;";
    rc = sqlite3_prepare_v2(db, sql_query, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Erreur préparation requête : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    printf("[+] Lancement du stress-test : %d requêtes de lecture aléatoires ciblées X/Z...\n", num_queries);
    
    clock_t start_time = clock();
    unsigned int x32 = (unsigned int)time(NULL);
    int found_count = 0;

    for (int i = 0; i < num_queries; i++) {
        x32 ^= x32 << 13; x32 ^= x32 >> 17; x32 ^= x32 << 5;
        int rand_x = x32 % max_chunks_x;

        x32 ^= x32 << 13; x32 ^= x32 >> 17; x32 ^= x32 << 5;
        int rand_z = x32 % max_chunks_z;

        sqlite3_bind_int(stmt, 1, rand_x);
        sqlite3_bind_int(stmt, 2, rand_z);

        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            found_count++;
        }
        sqlite3_reset(stmt);
    }

    clock_t end_time = clock();
    double time_taken = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    printf("[+] Stress-test finalisé.\n");
    printf("    -> Requêtes exécutées : %d\n", num_queries);
    printf("    -> Chunks trouvés     : %d\n", found_count);
    printf("    -> Temps total        : %.3f secondes\n", time_taken);
    printf("    -> Performance        : %.0f requêtes/seconde\n", (double)num_queries / time_taken);

    return 0;
}

// Commande : INITW
int cmd_initw() {
    if (ensure_db_dir() != 0) return 1;

    sqlite3 *db;
    char *err_msg = 0;
    char db_path[512];
    get_db_path(db_path, ZYN_DB_WORLD);

    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Impossible d'ouvrir la base : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    printf("[+] Connexion à %s réussie.\n", db_path);

    // Initialisation de la table avec notre structure optimisée (Champs typés INTEGER pour de légères charges)
    const char *sql_create_table = 
        "CREATE TABLE IF NOT EXISTS macro_chunks ("
        "    chunk_x INTEGER,"
        "    chunk_z INTEGER,"
        "    biome_type INTEGER NOT NULL,"
        "    temperature INTEGER,"
        "    humidity INTEGER,"
        "    max_elevation INTEGER,"
        "    PRIMARY KEY (chunk_x, chunk_z)"
        ");";

    rc = sqlite3_exec(db, sql_create_table, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Erreur SQL lors de la création de la table : %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }

    printf("[+] Table 'macro_chunks' initialisée avec index composite (X, Z) avec succès.\n");
    sqlite3_close(db);
    return 0;
}

// Commande : add-macro <x> <z> <biome> <temp> <hum> <elev_m>
int cmd_add_macro(int x, int z, int biome, double temp, double hum, double elev_m) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    char db_path[512];
    get_db_path(db_path, ZYN_DB_WORLD);

    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Impossible d'ouvrir la base : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    const char *sql_insert = 
        "INSERT OR REPLACE INTO macro_chunks (chunk_x, chunk_z, biome_type, temperature, humidity, max_elevation) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Erreur lors de la préparation de la requête : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    // Packing à la volée avant écriture en base de données
    uint8_t temp_raw = FLOAT_TO_RAW(temp);
    uint8_t hum_raw = FLOAT_TO_RAW(hum);
    int16_t elev_dm = M_TO_DM(elev_m);

    sqlite3_bind_int(stmt, 1, x);
    sqlite3_bind_int(stmt, 2, z);
    sqlite3_bind_int(stmt, 3, biome);
    sqlite3_bind_int(stmt, 4, temp_raw);
    sqlite3_bind_int(stmt, 5, hum_raw);
    sqlite3_bind_int(stmt, 6, elev_dm);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[-] Erreur lors de l'insertion du macro-chunk : %s\n", sqlite3_errmsg(db));
    } else {
        printf("[+] Macro-chunk (%d, %d) enregistré. Biome: %d, Alt: %.1fm (Packed: %d dm).\n", x, z, biome, elev_m, elev_dm);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return (rc == SQLITE_DONE) ? 0 : 1;
}

// Commande : INFO
int cmd_info() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    char db_path[512];
    get_db_path(db_path, ZYN_DB_WORLD);

    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Impossible d'ouvrir la base : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    printf("==================================================\n");
    printf("   Zynthar - Informations Base : %s\n", ZYN_DB_WORLD);
    printf("==================================================\n");
    printf("[*] Chemin absolu : %s\n\n", db_path);

    const char *sql_tables = "SELECT name, sql FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%';";
    
    rc = sqlite3_prepare_v2(db, sql_tables, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Erreur inspection tables : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    int table_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        table_count++;
        const unsigned char *table_name = sqlite3_column_text(stmt, 0);
        const unsigned char *table_sql = sqlite3_column_text(stmt, 1);

        printf("[Table #%d] : %s\n", table_count, table_name);
        printf("----------- Schéma SQL -----------\n%s\n----------------------------------\n", table_sql);

        sqlite3_stmt *count_stmt;
        char sql_count[128];
        sprintf(sql_count, "SELECT COUNT(*) FROM %s;", table_name);

        if (sqlite3_prepare_v2(db, sql_count, -1, &count_stmt, 0) == SQLITE_OK) {
            if (sqlite3_step(count_stmt) == SQLITE_ROW) {
                int rows = sqlite3_column_int(count_stmt, 0);
                printf("[*] Nombre d'enregistrements : %d\n\n", rows);
            }
            sqlite3_finalize(count_stmt);
        }
    }

    if (table_count == 0) {
        printf("[!] Aucune table trouvée. Initialisez-la avec 'initw'.\n");
    }

    printf("==================================================\n");
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "initw") == 0) {
        return cmd_initw();
    }
    else if (strcmp(argv[1], "add-macro") == 0) {
        if (argc < 8) {
            fprintf(stderr, "[-] Erreur : Paramètres manquants pour add-macro.\n");
            print_usage(argv[0]);
            return 1;
        }
        int x = atoi(argv[2]);
        int z = atoi(argv[3]);
        int biome = atoi(argv[4]);
        double temp = atof(argv[5]);
        double hum = atof(argv[6]);
        double elev = atof(argv[7]); // Reçoit la valeur métrique directe du terminal

        return cmd_add_macro(x, z, biome, temp, hum, elev);
    }
    else if (strcmp(argv[1], "export-csv") == 0) {
        if (argc < 3) {
            fprintf(stderr, "[-] Erreur : Veuillez spécifier le fichier CSV de sortie.\n");
            print_usage(argv[0]);
            return 1;
        }
        return cmd_export_csv(argv[2]);
    }
    else if (strcmp(argv[1], "import-csv") == 0) {
        if (argc < 3) {
            fprintf(stderr, "[-] Erreur : Fichier CSV à importer manquant.\n");
            print_usage(argv[0]);
            return 1;
        }
        return cmd_import_csv(argv[2]);
    }
    else if (strcmp(argv[1], "populate-random") == 0) {
        return cmd_populate_random();
    }
    else if (strcmp(argv[1], "stress-test") == 0) {
        return cmd_stress_test();
    }
    else if (strcmp(argv[1], "info") == 0) {
        return cmd_info();
    }
    else {
        fprintf(stderr, "[-] Commande inconnue : %s\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
