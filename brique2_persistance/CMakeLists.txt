# =============================================================================
# ZYNTHAR v0.1 - CONFIGURATION CMAKE - BRIQUE 2 
# =============================================================================

# --- Détection des Dépendances ---
find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

# --- Cible Éxecutable Autonome : zyn_cerbere ---
add_executable(zyn_cerbere src/Cerbere/zyn_cerbere.c src/Chronos/zyn_chronos_utils.c)
target_link_libraries(zyn_cerbere PRIVATE SQLite::SQLite3 Threads::Threads zynthar_shared rt m)

# 🎯 AJOUT : Spécifie au compilateur d'inclure le dossier include/ pour zyn_cerbere
target_include_directories(zyn_cerbere PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src/Chronos)

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
add_executable(zyn_chronos src/Chronos/zyn_chronos.c src/Chronos/zyn_chronos_utils.c)
# 🎯 CORRECTION 3 : Utilisation de la cible moderne SQLite::SQLite3 initialisée par find_package
target_link_libraries(zyn_chronos PRIVATE SQLite::SQLite3 zynthar_shared rt m)
target_include_directories(zyn_chronos PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(zyn_atropos src/Atropos/zyn_atropos.c src/Atropos/zyn_atropos_utils.c)
target_link_libraries(zyn_atropos PRIVATE zynthar_shared rt Threads::Threads m)
target_include_directories(zyn_atropos PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# --- Cible 4 : zyn_forgeron ---
add_executable(zyn_forgeron src/Forgeron/zyn_forgeron.c src/Forgeron/zyn_forgeron_utils.c )
target_link_libraries(zyn_forgeron PRIVATE zynthar_shared rt Threads::Threads m)
target_include_directories(zyn_forgeron PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(zyn_atlas src/Atlas/zyn_atlas.c src/Atlas/zyn_atlas_utils.c src/Chronos/zyn_chronos_utils.c)
target_link_libraries(zyn_atlas PRIVATE SQLite::SQLite3 zynthar_shared rt Threads::Threads m)
target_include_directories(zyn_atlas PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src/Chronos)

add_executable(zyn_hecate src/Hecate/zyn_hecate.c src/Hecate/zyn_hecate_utils.c)
target_link_libraries(zyn_hecate PRIVATE SQLite::SQLite3 zynthar_shared rt Threads::Threads m)
target_include_directories(zyn_hecate PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
