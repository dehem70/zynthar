# =============================================================================
# ZYNTHAR v0.1 - CONFIGURATION CMAKE - BRIQUE 2 
# =============================================================================

# --- Détection des Dépendances ---
find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

# Cela regroupe l'ensemble de nos briques algorithmiques pour qu'elles soient réutilisables
add_library(zyn_sqlite_bindings STATIC
    src/Cerbere/zyn_cerbere.c
    src/Chronos/zyn_chronos.c
)

# 🎯 CORRECTION 2 : La bibliothèque statique a AUSSI besoin de savoir où est zynthar.h
# On lui lie également SQLite3 car Chronos y fait directement référence
target_link_libraries(zyn_sqlite_bindings PUBLIC zynthar_shared SQLite::SQLite3 PRIVATE Threads::Threads m)

# --- Cible Éxecutable Autonome : zyn_cerbere ---
add_executable(zyn_cerbere src/Cerbere/zyn_cerbere.c)
target_link_libraries(zyn_cerbere PRIVATE Threads::Threads zynthar_shared m)

# --- Maximize Performance : Attribution automatique de la capacité CAP_SYS_ADMIN ---
find_program(SETCAP_EXECUTABLE NAMES setcap HINTS /usr/sbin /sbin /usr/bin)
if(NOT SETCAP_EXECUTABLE)
    set(SETCAP_EXECUTABLE "/usr/sbin/setcap")
endif()

add_custom_command(TARGET zyn_cerbere POST_BUILD
    COMMAND sudo ${SETCAP_EXECUTABLE} "cap_sys_admin+ep" $<TARGET_FILE:zyn_cerbere>
    COMMENT "🔒 Attribution de la capacité CAP_SYS_ADMIN à zyn_cerbere"
)

# --- Cible 2 : zyn_chronos ---
add_executable(zyn_chronos src/Chronos/zyn_chronos.c)
# 🎯 CORRECTION 3 : Utilisation de la cible moderne SQLite::SQLite3 initialisée par find_package
target_link_libraries(zyn_chronos PRIVATE SQLite::SQLite3 zynthar_shared m)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
