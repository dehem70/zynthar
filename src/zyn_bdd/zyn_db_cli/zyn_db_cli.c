//////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                      //
//                                          ZYNTHAR v0.1                                                //
//                                                                                                      //
// Auteur : Dehem70                                                                                     //
// Date   : 27/05/2026                                                                                  //
//                                                                                                      //
// zyn_db_cli  ; programme de gestion des bases de données                                              //
// utilisation : zyn_db_cli cmd [options]                                                               //
//                                                                                                      //
//     cmd     : initw => initialisation de la base de données zyn_world.db                             //
//               add-macro <x> <y> <biome> <temp> <hum> <elev> => Ajoute ou met à jour un macro-chunk   //
//                                                                                                      //
// nota : les chemins sont consiédés avec le préfixe enregistré dans la variable d'environnement        //
//        ZYNTHAR_DB_DIR                                                                                //
//                                                                                                      //
//////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

// Commande : initw

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
    else {
        fprintf(stderr, "[-] Commande inconnue : %s\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
