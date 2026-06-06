# =============================================================================
# ZYNTHAR v0.1 - CONFIGURATION CMAKE - BRIQUE 5 (OUTILS OFFLINE)
# =============================================================================
find_package(SQLite3 REQUIRED)
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
    src/macro_procedural/zyn_river_agent.c
    src/macro_procedural/zyn_river_agent.h
    src/macro_procedural/zyn_river_stepper.c
    src/macro_procedural/zyn_river_stepper.h
    src/macro_procedural/zyn_river_system_pipeline.c
    src/macro_procedural/zyn_river_system_pipeline.h
    src/macro_procedural/zyn_gen_map_biome.c
    src/macro_procedural/zyn_gen_map_biome.h
    src/macro_procedural/zyn_gen_map_macro.c
    src/macro_procedural/zyn_gen_map_macro.h
    src/macro_procedural/zyn_gen_png.c
    src/macro_procedural/zyn_gen_png.h
    src/macro_procedural/zyn_test_framework.c
    src/macro_procedural/zyn_test_framework.h
    src/macro_procedural/zyn_utils.c
    src/macro_procedural/zyn_utils.h
    src/bulk_injector/zyn_macro_chunk_injector.h
    src/bulk_injector/zyn_macro_chunk_injector.c
)
find_package(OpenMP REQUIRED)
if(OpenMP_FOUND)
    target_link_libraries(zyn_procedural_offline PRIVATE OpenMP::OpenMP_CXX)
    # ou target_compile_options s'il s'agit de C pur :
    target_compile_options(zyn_procedural_offline PRIVATE ${OpenMP_C_FLAGS})
endif()
# Indiquer à CMake où trouver les en-têtes locaux de la brique procédurale
target_include_directories(zyn_procedural_offline PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/macro_procedural
)

# Liaison de la bibliothèque avec l'en-tête partagé global (zynthar.h)
target_link_libraries(zyn_procedural_offline PRIVATE zynthar_shared)

# Stress-test unitaire des rivières
add_executable(zyn_test src/macro_procedural/zyn_test.c)
target_link_libraries(zyn_test PRIVATE zyn_procedural_offline zynthar_shared m)
set_target_properties(zyn_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/brique5_outils_offline")

# Déclaration de l'exécutable principal (on remonte d'un dossier avec ../)
add_executable(zyn_map_generator src/zyn_map_generator.c)

# Configuration des chemins d'inclusion en remontant également d'un dossier
target_include_directories(zyn_map_generator PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/macro_procedural
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bulk_injector
    ${SQLite3_INCLUDE_DIRS}
)

# Liaison des dépendances (les cibles CMake comme zyn_procedural_offline restent valides)
target_link_libraries(zyn_map_generator PRIVATE 
    zyn_procedural_offline
    ${SQLite3_LIBRARIES}
    zynthar_shared
    m
)

# Configuration du répertoire de sortie pour l'exécutable généré
set_target_properties(zyn_map_generator PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/brique5_outils_offline"
)

