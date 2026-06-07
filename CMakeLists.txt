# Configuration globale des optimisations de vitesse pour GCC/Clang
project(ZYNTHAR)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choix du type de build" FORCE)
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Ajoute des optimisations spécifiques au mode Release mondial
    set(CMAKE_C_FLAGS_RELEASE "-O3 -march=native -ffast-math")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -ffast-math")
endif()

# Déclaration de la cible d'en-tête partagée (Source de vérité)
add_library(zynthar_shared INTERFACE)
# On indique à CMake où trouver les fichiers de cette cible
target_include_directories(zynthar_shared INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/shared/include)


add_subdirectory(brique7_maintenance)
add_subdirectory(brique5_outils_offline)
add_subdirectory(brique2_persistance)


