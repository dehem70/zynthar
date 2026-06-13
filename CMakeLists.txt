# Configuration globale des optimisations de vitesse pour GCC/Clang
cmake_minimum_required(VERSION 3.28) # Force l'activation des comportements modernes

project(ZYNTHAR)

cmake_policy(SET CMP0069 NEW)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choix du type de build" FORCE)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT lto_supported OUTPUT error)
if(lto_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    message(STATUS "LTO / IPO est activé avec succès pour le mode Release !")
else()
    message(WARNING "LTO non supporté : ${error}")
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Ajoute des optimisations spécifiques au mode Release mondial
    set(CMAKE_C_FLAGS_RELEASE "-Ofast -march=native -fvect-cost-model=unlimited")
    set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -march=native -fvect-cost-model=unlimited")
endif()

# Déclaration de la cible d'en-tête partagée (Source de vérité)
add_library(zynthar_shared INTERFACE)
# On indique à CMake où trouver les fichiers de cette cible
target_include_directories(zynthar_shared INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/shared/include)


add_subdirectory(brique7_maintenance)
add_subdirectory(brique5_outils_offline)
add_subdirectory(brique2_persistance)


