# =============================================================================
# ZYNTHAR v0.1 - CONFIGURATION CMAKE - BRIQUE 2 
# =============================================================================
set(CMAKE_BUILD_TYPE Debug)
# --- Détection des Dépendances ---
find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

# --- Cible Éxecutable Autonome : zyn_cerbere ---
add_executable(zyn_cerbere src/Cerbere/zyn_cerbere.c)
target_link_libraries(zyn_cerbere PRIVATE Threads::Threads zynthar_shared rt m)

# 🎯 AJOUT : Spécifie au compilateur d'inclure le dossier include/ pour zyn_cerbere
target_include_directories(zyn_cerbere PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

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
add_executable(zyn_chronos src/Chronos/zyn_chronos.c )
# 🎯 CORRECTION 3 : Utilisation de la cible moderne SQLite::SQLite3 initialisée par find_package
target_link_libraries(zyn_chronos PRIVATE SQLite::SQLite3 zynthar_shared rt m)
# 🎯 AJOUT : Spécifie au compilateur d'inclure le dossier include/ pour zyn_cerbere
target_include_directories(zyn_chronos PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(zyn_atropos src/Atropos/zyn_atropos.c src/Atropos/zyn_atropos_utils.c)
target_link_libraries(zyn_atropos PRIVATE zynthar_shared rt Threads::Threads m)
target_include_directories(zyn_atropos PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# --- Cible 4 : zyn_forgeron ---
add_executable(zyn_forgeron src/Forgeron/zyn_forgeron.c src/Forgeron/zyn_forgeron_utils.c )
target_link_libraries(zyn_forgeron PRIVATE zynthar_shared rt Threads::Threads m)
target_include_directories(zyn_forgeron PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(zyn_atlas src/Atlas/zyn_atlas.c src/Atlas/zyn_atlas_utils.c)
target_link_libraries(zyn_atlas PRIVATE zynthar_shared rt Threads::Threads m)
target_include_directories(zyn_atlas PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
