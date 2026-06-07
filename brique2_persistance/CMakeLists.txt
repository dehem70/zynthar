# =============================================================================
# ZYNTHAR v0.1 - CONFIGURATION CMAKE - BRIQUE 2 
# =============================================================================
find_package(SQLite3 REQUIRED)
# Cela regroupe l'ensemble de nos briques algorithmiques pour qu'elles soient réutilisables
add_library(zyn_sqlite_bindings STATIC
    src/sqlite_bindings/zynthar_db.c
    src/sqlite_bindings/zynthar_db.h
    src/macro_access/macro_access.c
    src/macro_access/macro_access.h
)
# Indiquer à CMake où trouver les en-têtes locaux de la brique procédurale
target_include_directories(zyn_sqlite_bindings PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sqlite_bindings
    ${CMAKE_CURRENT_SOURCE_DIR}/src/macro_access
    ${SQLite3_INCLUDE_DIRS}
)

# Liaison de la bibliothèque avec l'en-tête partagé global (zynthar.h)
target_link_libraries(zyn_sqlite_bindings 
    PRIVATE 
        zynthar_shared
    PUBLIC
        ${SQLite3_LIBRARIES}
)

# Stress-test brique2
add_executable(zyn_brique2_stress src/zyn_brique2_stress.c)
target_link_libraries(zyn_brique2_stress PRIVATE zyn_sqlite_bindings zynthar_shared m )
set_target_properties(zyn_brique2_stress PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/brique2_persistance")
target_include_directories(zyn_brique2_stress PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sqlite_bindings
    ${SQLite3_INCLUDE_DIRS}
)

