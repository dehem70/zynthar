# Déclaration de la cible d'en-tête partagée (Source de vérité)
add_library(zynthar_shared INTERFACE)

# On indique à CMake où trouver les fichiers de cette cible
target_include_directories(zynthar_shared INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/shared/include)
