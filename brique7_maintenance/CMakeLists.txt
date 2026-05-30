# brique7_maintenance/CMakeLists.txt

# 1. Recherche obligatoire de la bibliothèque SQLite3 installée sur le système
find_package(SQLite3 REQUIRED)

# 2. Déclaration des sources pour l'outil de gestion de base de données
set(DB_CLI_SOURCES
    src/db_manager/zyn_db_cli.c
)

# 3. Déclaration de l'exécutable final
add_executable(zyn_db_cli ${DB_CLI_SOURCES})

# 4. Liaison avec les dossiers d'inclusion (headers de SQLite3 et zynthar_shared)
target_link_libraries(zyn_db_cli 
    PRIVATE 
        zynthar_shared
        ${SQLite3_LIBRARIES}
)

# 5. Définition du dossier de sortie pour que le binaire soit généré proprement
set_target_properties(zyn_db_cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/brique7_maintenance"
)
