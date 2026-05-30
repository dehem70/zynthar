////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                        //
//                                          ZYNTHAR v0.1                                                  //
//                                                                                                        //
// Auteur : Dehem70                                                                                       //
// Date   : 27/05/2026                                                                                    //
//                                                                                                        //
// zyn_db_cli  ; programme de gestion des bases de données                                                //
// utilisation : zyn_db_cli cmd [options]                                                                 //
//                                                                                                        //
//     cmd     : initw => initialisation de la base de données zyn_world.db                               //
//               add-macro <x> <y> <biome> <temp> <hum> <elev> => Ajoute ou met à jour un macro-chunk     //
//               export-csv <fichier.csv> => exporte la base dans fichier.csv                             //
//               import-csv <fichier.csv> => Importe un fichier CSV dans macro_chunks (écrase l'existant) //
//               populate-random => Remplit la grille du monde de données aléatoires                      //
//               stress-test => Exécute 10 000 requêtes de lecture aléatoires                             //
//               info => Affiche la structure et le nombre de lignes de la base                           //
//                                                                                                        //
// nota : les chemins sont considérés avec le préfixe enregistré dans la variable d'environnement         //
//        ZYNTHAR_DB_DIR                                                                                  //
//                                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>

// En-têtes requis sous Linux/POSIX pour manipuler 'struct stat' et 'mkdir'
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <../../include/zynthar.h>

// Fonction d'aide pour afficher l'usage du CLI
void print_usage(const char *prog_name) {
    printf("Usage: %s <commande> [options]\n", prog_name);
    printf("Commandes disponibles :\n");
    printf("  initw       Crée la base de données et la table macro_chunks\n");
    printf("  add-macro <x> <y> <biome> <temp> <hum> <elev>  Ajoute ou met à jour un macro-chunk\n");
    printf("  export-csv <fichier.csv>     Exporte la table macro_chunks en format CSV\n");
    printf("  import-csv <fichier.csv>     Importe un fichier CSV dans macro_chunks (écrase l'existant)\n");
    printf("  populate-random              Remplit la grille du monde de données aléatoires\n");
    printf("  stress-test                  Exécute 10 000 requêtes de lecture aléatoires\n");
    printf("  info                         Affiche la structure et le nombre de lignes de la base\n");

}

// Fonction utilitaire pour générer le chemin complet de la base de données
void get_db_path(char *dest, const char *db_name) {
    char *root_env = getenv("ZYNTHAR_ROOT");

    if (root_env != NULL) {
        // Si ZYNTHAR_ROOT est configuré, on forge le chemin absolu propre
        // Exemple : /home/user/zynthar/db/zyn-world.db
        sprintf(dest, "%s/%s%s", root_env, ZYN_DB_EMPLACEMENT, db_name);
    } else {
        // Repli (Fallback) sur le répertoire courant si la variable est absente
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
    char db_path[256];
    get_db_path(db_path, ZYN_DB_WORLD);

    // 1. Ouverture de la base de données
    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Impossible d'ouvrir la base : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    // 2. Ouverture du fichier CSV en écriture
    FILE *csv_file = fopen(filename, "w");
    if (csv_file == NULL) {
        fprintf(stderr, "[-] Erreur : Impossible de créer le fichier %s\n", filename);
        sqlite3_close(db);
        return 1;
    }

    // 3. Écriture de l'en-tête (headers) du CSV
    fprintf(csv_file, "chunk_x,chunk_y,biome_type,temperature,humidity,max_elevation\n");

    // 4. Préparation de la requête de lecture globale
    const char *sql_select = "SELECT chunk_x, chunk_y, biome_type, temperature, humidity, max_elevation FROM macro_chunks;";
    rc = sqlite3_prepare_v2(db, sql_select, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Erreur lors de la préparation de la lecture : %s\n", sqlite3_errmsg(db));
        fclose(csv_file);
        sqlite3_close(db);
        return 1;
    }

    int row_count = 0;
    // 5. Boucle de lecture des résultats
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int x = sqlite3_column_int(stmt, 0);
        int y = sqlite3_column_int(stmt, 1);
        int biome = sqlite3_column_int(stmt, 2);
        double temp = sqlite3_column_double(stmt, 3);
        double hum = sqlite3_column_double(stmt, 4);
        double elev = sqlite3_column_double(stmt, 5);

        // Écriture de la ligne au format CSV (séparateur virgule)
        fprintf(csv_file, "%d,%d,%d,%.4f,%.4f,%.2f\n", x, y, biome, temp, hum, elev);
        row_count++;
    }

    // 6. Nettoyage et fermeture
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
    char db_path[256];
    get_db_path(db_path, ZYN_DB_WORLD);

    // 1. Ouverture de la base
    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Impossible d'ouvrir la base : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    // 2. Vérification si la table contient déjà des données
    const char *sql_count = "SELECT COUNT(*) FROM macro_chunks;";
    int count = 0;
    rc = sqlite3_prepare_v2(db, sql_count, -1, &stmt, 0);
    if (rc == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    // 3. Demande de confirmation si la base n'est pas vide
    if (count > 0) {
        printf("[!] Attention : La table macro_chunks contient déjà %d enregistrement(s).\n", count);
        printf("[?] Voulez-vous TOUT effacer et importer ce CSV ? (y/N) : ");
        fflush(stdout);
        
        char choice = getchar();
        // Consommer le reste de la ligne dans le tampon (le '\n')
        if (choice != '\n' && choice != EOF) {
            while (getchar() != '\n');
        }

        if (choice != 'y' && choice != 'Y') {
            printf("[-] Opération annulée par l'utilisateur.\n");
            sqlite3_close(db);
            return 0;
        }

        // Vider la table
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

    // 4. Ouverture du fichier CSV pour lecture
    FILE *csv_file = fopen(filename, "r");
    if (csv_file == NULL) {
        fprintf(stderr, "[-] Erreur : Impossible d'ouvrir le fichier %s\n", filename);
        sqlite3_close(db);
        return 1;
    }

    // 5. Préparation de la requête d'insertion (avec un INSERT classique puisque la table est vide ou purgée)
    const char *sql_insert = 
        "INSERT INTO macro_chunks (chunk_x, chunk_y, biome_type, temperature, humidity, max_elevation) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    
    rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Erreur préparation insertion : %s\n", sqlite3_errmsg(db));
        fclose(csv_file);
        sqlite3_close(db);
        return 1;
    }

    // 6. Lancement d'une TRANSACTION pour optimiser la vitesse d'écriture
    sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, 0);

    char line[512];
    int line_count = 0;
    int imported_count = 0;

    // Lecture ligne par ligne
    while (fgets(line, sizeof(line), csv_file)) {
        line_count++;
        
        // Ignorer la première ligne (l'en-tête du CSV)
        if (line_count == 1 && strstr(line, "chunk_x") != NULL) {
            continue;
        }

        int x, y, biome;
        double temp, hum, elev;

        // Extraction des données selon le format : %d,%d,%d,%lf,%lf,%lf
        if (sscanf(line, "%d,%d,%d,%lf,%lf,%lf", &x, &y, &biome, &temp, &hum, &elev) == 6) {
            sqlite3_reset(stmt);
            sqlite3_bind_int(stmt, 1, x);
            sqlite3_bind_int(stmt, 2, y);
            sqlite3_bind_int(stmt, 3, biome);
            sqlite3_bind_double(stmt, 4, temp);
            sqlite3_bind_double(stmt, 5, hum);
            sqlite3_bind_double(stmt, 6, elev);

            if (sqlite3_step(stmt) == SQLITE_DONE) {
                imported_count++;
            }
        } else {
            fprintf(stderr, "[*] Alerte : Ligne %d mal formatée ignorée.\n", line_count);
        }
    }

    // 7. Validation de la transaction et fermeture
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
    char db_path[256];
    get_db_path(db_path, ZYN_DB_WORLD);

    // 1. Calcul des dimensions de la grille de macro-chunks
    int max_chunks_x = ZYN_X_MAX / ZYN_CHUNK_MACRO_DIM;
    int max_chunks_y = ZYN_Y_MAX / ZYN_CHUNK_MACRO_DIM;
    long long total_chunks = (long long)max_chunks_x * max_chunks_y;

    printf("[*] Configuration du monde détectée :\n");
    printf("    -> Grille de Macro-Chunks : %d x %d\n", max_chunks_x, max_chunks_y);
    printf("    -> Total à générer : %lld macro-chunks\n", total_chunks);

    // 2. Demande de confirmation
    printf("[?] Confirmez-vous la génération aléatoire de la base (effacement des données présentes) ? (y/N) : ");
    fflush(stdout);
    char choice = getchar();
    if (choice != '\n' && choice != EOF) { while (getchar() != '\n'); }
    if (choice != 'y' && choice != 'Y') {
        printf("[-] Opération annulée.\n");
        return 0;
    }

    // 3. Ouverture et purge de la table existante
    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Impossible d'ouvrir la base : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    // --- Enclenchement du Chronomètre ---
    clock_t start_time = clock();

    // --- [OPTI 1] PRAGMA DE CONFIGURATION CRITIQUES ---
    // Désactive la synchronisation disque pendant l'écriture (très safe ici car c'est de la génération brute)
    sqlite3_exec(db, "PRAGMA synchronous = OFF;", 0, 0, 0);
    // Utilise un fichier journal en mémoire plutôt que sur le disque
    sqlite3_exec(db, "PRAGMA journal_mode = MEMORY;", 0, 0, 0);
    // Augmente la taille du cache mémoire de SQLite (ici ~80 Mo de cache)
    sqlite3_exec(db, "PRAGMA cache_size = -80000;", 0, 0, 0);
    // Optimisation de stockage pour les insertions consécutives
    sqlite3_exec(db, "PRAGMA locking_mode = EXCLUSIVE;", 0, 0, 0);


    sqlite3_exec(db, "DELETE FROM macro_chunks;", 0, 0, 0);

    // 4. Préparation de la requête d'insertion
    const char *sql_insert = 
        "INSERT INTO macro_chunks (chunk_x, chunk_y, biome_type, temperature, humidity, max_elevation) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Erreur préparation : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    // Initialisation de la graine aléatoire de l'horloge
    unsigned int x32 = (unsigned int)time(NULL); 
    double range = ZYN_Z_MAX - ZYN_Z_MIN;

    printf("[+] Génération flash en cours...\n");
    
    // 5. Début de la Transaction globale
    sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, 0);

    long long inserted = 0;
    for (int x = 0; x < max_chunks_x; x++) {
    	sqlite3_bind_int(stmt, 1, x); 
        
        for (int y = 0; y < max_chunks_y; y++) {
            // Algorithme de Xorshift pour générer un entier pseudo-aléatoire à haute vitesse
            x32 ^= x32 << 13;
            x32 ^= x32 >> 17;
            x32 ^= x32 << 5;

            int biome = x32 % 6;
            double temp = (double)(x32 % 400) * 0.1;
            double hum = (double)(x32 % 100) * 0.01;
            double elev = ZYN_Z_MIN + ((double)(x32 % 10000) * 0.0001) * range;

            sqlite3_bind_int(stmt, 2, y);
            sqlite3_bind_int(stmt, 3, biome);
            sqlite3_bind_double(stmt, 4, temp);
            sqlite3_bind_double(stmt, 5, hum);
            sqlite3_bind_double(stmt, 6, elev);

            sqlite3_step(stmt);
            sqlite3_reset(stmt);
            
            inserted++;
        }
        // Afficher une barre de progression textuelle toutes les 100 lignes X
        if (x % 100 == 0) {
            printf("\r    Progression : %.1f%%", ((double)inserted / total_chunks) * 100.0);
            fflush(stdout);
        }
    }

    // 6. Validation finale
    sqlite3_exec(db, "COMMIT;", 0, 0, 0);
    sqlite3_finalize(stmt);
    // Remettre la base en mode normal à la fermeture1
    sqlite3_exec(db, "PRAGMA locking_mode = NORMAL;", 0, 0, 0);
    sqlite3_close(db);

    clock_t end_time = clock();
    double time_taken = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

    printf("[+] Terminé ! %lld macro-chunks générés en %.3f secondes.\n", inserted, time_taken);
    return 0;
}
// Commande : initw

// Commande : STRESS-TEST
int cmd_stress_test() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    char db_path[256];
    get_db_path(db_path, ZYN_DB_WORLD);

    // Bornes de la grille de macro-chunks
    int max_chunks_x = ZYN_X_MAX / ZYN_CHUNK_MACRO_DIM;
    int max_chunks_y = ZYN_Y_MAX / ZYN_CHUNK_MACRO_DIM;
    int num_queries = 10000;

    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Erreur ouverture : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    // Optimisations de lecture
    sqlite3_exec(db, "PRAGMA cache_size = -80000;", 0, 0, 0); // 80 Mo Cache
    sqlite3_exec(db, "PRAGMA read_uncommitted = TRUE;", 0, 0, 0);

    // Préparation de la requête ciblée
    const char *sql_query = "SELECT biome_type, max_elevation FROM macro_chunks WHERE chunk_x = ? AND chunk_y = ?;";
    rc = sqlite3_prepare_v2(db, sql_query, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Erreur préparation requête : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    printf("[+] Lancement du stress-test : %d requêtes de lecture aléatoires...\n", num_queries);
    
    // Top Chrono
    clock_t start_time = clock();

    unsigned int x32 = (unsigned int)time(NULL);
    int found_count = 0;

    for (int i = 0; i < num_queries; i++) {
        // Xorshift pour générer des coordonnées X et Y dans la grille du monde
        x32 ^= x32 << 13; x32 ^= x32 >> 17; x32 ^= x32 << 5;
        int rand_x = x32 % max_chunks_x;

        x32 ^= x32 << 13; x32 ^= x32 >> 17; x32 ^= x32 << 5;
        int rand_y = x32 % max_chunks_y;

        // Liaison des coordonnées aléatoires
        sqlite3_bind_int(stmt, 1, rand_x);
        sqlite3_bind_int(stmt, 2, rand_y);

        // Exécution de la requête
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            found_count++; // Le chunk existe en base
        }

        // Réinitialisation du statement pour la prochaine itération
        sqlite3_reset(stmt);
    }

    // Fin Chrono
    clock_t end_time = clock();
    double time_taken = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    printf("[+] Stress-test terminé !\n");
    printf("    -> Requêtes exécutées : %d\n", num_queries);
    printf("    -> Chunks trouvés     : %d\n", found_count);
    printf("    -> Temps total        : %.3f secondes\n", time_taken);
    printf("    -> Performance        : %.0f requêtes/seconde\n", (double)num_queries / time_taken);

    return 0;
}


int cmd_initw() {
    if (ensure_db_dir() != 0) return 1;

    sqlite3 *db;
    char *err_msg = 0;
    char db_path[256];
    get_db_path(db_path, ZYN_DB_WORLD);

    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Impossible d'ouvrir la base : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    printf("[+] Connexion à %s réussie.\n", db_path);

    const char *sql_create_table = 
        "CREATE TABLE IF NOT EXISTS macro_chunks ("
        "    chunk_x INTEGER,"
        "    chunk_y INTEGER,"
        "    biome_type INTEGER NOT NULL,"
        "    temperature REAL,"
        "    humidity REAL,"
        "    max_elevation REAL,"
        "    PRIMARY KEY (chunk_x, chunk_y)"
        ");";

    rc = sqlite3_exec(db, sql_create_table, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Erreur SQL lors de la création de la table : %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }

    printf("[+] Table 'macro_chunks' initialisée avec succès.\n");
    sqlite3_close(db);
    return 0;
}

// Commande : add-macro <x> <y> <biome> <temp> <hum> <elev>
int cmd_add_macro(int x, int y, int biome, double temp, double hum, double elev) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    char db_path[256];
    get_db_path(db_path, ZYN_DB_WORLD);

    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Impossible d'ouvrir la base : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    // Utilisation d'un INSERT OR REPLACE pour mettre à jour si le chunk existe déjà
    const char *sql_insert = 
        "INSERT OR REPLACE INTO macro_chunks (chunk_x, chunk_y, biome_type, temperature, humidity, max_elevation) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    // Préparation de la requête (évite les injections et plus performant)
    rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Erreur lors de la préparation de la requête : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    // Liaison (binding) des variables
    sqlite3_bind_int(stmt, 1, x);
    sqlite3_bind_int(stmt, 2, y);
    sqlite3_bind_int(stmt, 3, biome);
    sqlite3_bind_double(stmt, 4, temp);
    sqlite3_bind_double(stmt, 5, hum);
    sqlite3_bind_double(stmt, 6, elev);

    // Exécution
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[-] Erreur lors de l'insertion du macro-chunk : %s\n", sqlite3_errmsg(db));
    } else {
        printf("[+] Macro-chunk (%d, %d) enregistré avec succès (Biome: %d).\n", x, y, biome);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return (rc == SQLITE_DONE) ? 0 : 1;
}

// Commande : INFO
int cmd_info() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    char db_path[256];
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

    // 1. Requête sur sqlite_master pour lister les tables et leur structure
    const char *sql_tables = "SELECT name, sql FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%';";
    
    rc = sqlite3_prepare_v2(db, sql_tables, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[-] Erreur inspection tables : %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    int table_count = 0;
    
    // Pour chaque table trouvée
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        table_count++;
        const unsigned char *table_name = sqlite3_column_text(stmt, 0);
        const unsigned char *table_sql = sqlite3_column_text(stmt, 1);

        printf("[Table #%d] : %s\n", table_count, table_name);
        printf("----------- Schéma SQL -----------\n%s\n----------------------------------\n", table_sql);

        // 2. Requête dynamique pour compter le nombre d'enregistrements dans cette table
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
        printf("[!] Aucune table n'a été trouvée dans cette base. Initialisez-la avec 'init<x>'.\n");
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

    // Routage de la commande CLI
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
        int y = atoi(argv[3]);
        int biome = atoi(argv[4]);
        double temp = atof(argv[5]);
        double hum = atof(argv[6]);
        double elev = atof(argv[7]);

        return cmd_add_macro(x, y, biome, temp, hum, elev);
    }
    else if (strcmp(argv[1], "export-csv") == 0) {
        if (argc < 3) {
            fprintf(stderr, "[-] Erreur : Veuillez spécifier le nom du fichier CSV de sortie.\n");
            print_usage(argv[0]);
            return 1;
        }
        return cmd_export_csv(argv[2]);
    }
    else if (strcmp(argv[1], "import-csv") == 0) {
        if (argc < 3) {
            fprintf(stderr, "[-] Erreur : Nom du fichier CSV à importer manquant.\n");
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
