# =============================================================================
# ZYNTHAR v0.1 - CONFIGURATION CMAKE - BRIQUE 5 (OUTILS OFFLINE)
# =============================================================================

# 1. Déclaration de la bibliothèque statique du générateur macro-procédural
# Cela regroupe l'ensemble de nos briques algorithmiques pour qu'elles soient réutilisables
add_library(zyn_procedural_offline STATIC
    src/macro_procedural/zyn_noise.c
    src/macro_procedural/zyn_noise.h
    src/macro_procedural/zyn_gen_map_relief.c
    src/macro_procedural/zyn_gen_map_relief.h
    src/macro_procedural/zyn_gen_map_temperature.c
    src/macro_procedural/zyn_gen_map_temperature.h
    src/macro_procedural/zyn_gen_wind_global.c
    src/macro_procedural/zyn_gen_wind_global.h
    src/macro_procedural/zyn_gen_wind_local.c
    src/macro_procedural/zyn_gen_wind_local.h
    src/macro_procedural/zyn_gen_map_humidity.c
    src/macro_procedural/zyn_gen_map_humidity.h
    src/macro_procedural/zyn_gen_png.c
    src/macro_procedural/zyn_gen_png.h
)

# Indiquer à CMake où trouver les en-têtes locaux de la brique procédurale
target_include_directories(zyn_procedural_offline PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/macro_procedural
)

# Liaison de la bibliothèque avec l'en-tête partagé global (zynthar.h)
target_link_libraries(zyn_procedural_offline PRIVATE zynthar_shared)


# =============================================================================
# EXÉCUTABLE 1 : TEST & BENCHMARK DU MOTEUR DE BRUIT (zyn_test_noise)
# =============================================================================
add_executable(zyn_test_noise 
    src/macro_procedural/zyn_test_noise.c
)

# Liaison avec le moteur de bruit, l'include global et la bibliothèque mathématique standard (-lm)
target_link_libraries(zyn_test_noise 
    PRIVATE 
        zyn_procedural_offline
        zynthar_shared
        m
)

# Dossier de sortie du binaire de stress-test
set_target_properties(zyn_test_noise PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/brique5_outils_offline"
)


# =============================================================================
# EXÉCUTABLE 2 : PIPELINE DE GÉNÉRATION MONDIAL GLOBAL (zyn_test_gen_map_relief)
# =============================================================================
add_executable(zyn_test_gen_map_macro 
    src/macro_procedural/zyn_test_gen_map_macro.c
)

# Liaison avec notre bibliothèque géomorphologique complète, l'include global et la lib m
target_link_libraries(zyn_test_gen_map_macro 
    PRIVATE 
        zyn_procedural_offline
        zynthar_shared
        m
)

# =============================================================================
# EXÉCUTABLE 3 : RAPPORT ET TEST DU VENT GLOBAL MACRO (zyn_test_gen_wind_global)
# =============================================================================
add_executable(zyn_test_gen_wind_global 
    src/macro_procedural/zyn_test_gen_wind_global.c
)

# Liaison avec notre bibliothèque, les définitions globales et la lib m (cosf/sinf/sqrtf)
target_link_libraries(zyn_test_gen_wind_global 
    PRIVATE 
        zyn_procedural_offline
        zynthar_shared
        m
)

# =============================================================================
# EXÉCUTABLE 4 : STRESS-TEST & CONFORMITÉ VENT LOCAL (zyn_test_gen_wind_local)
# =============================================================================
add_executable(zyn_test_gen_wind_local 
    src/macro_procedural/zyn_test_gen_wind_local.c
)
target_link_libraries(zyn_test_gen_wind_local 
    PRIVATE 
        zyn_procedural_offline
        zynthar_shared
        m
)

# =============================================================================
# EXÉCUTABLE 5 : STRESS-TEST & CONFORMITÉ HUMIDITÉ (zyn_test_gen_humidity)
# =============================================================================
add_executable(zyn_test_gen_humidity src/macro_procedural/zyn_test_gen_humidity.c)
target_link_libraries(zyn_test_gen_humidity PRIVATE zyn_procedural_offline zynthar_shared m)
set_target_properties(zyn_test_gen_humidity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/brique5_outils_offline")

# Dossier de sortie du binaire du pipeline de génération de monde
set_target_properties(zyn_test_gen_map_macro PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/brique5_outils_offline"
)
