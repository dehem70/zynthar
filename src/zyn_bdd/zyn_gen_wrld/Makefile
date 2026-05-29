# =============================================================================
# ZYNTHAR PROJECT - MAKEFILE COMPILATION
# =============================================================================

CC       := gcc
# Récupère le chemin absolu du dossier où se trouve ce Makefile
ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

# Tu peux ensuite t'en servir pour définir des dossiers de build ou d'include cleanly
CFLAGS = -Wall -Wextra -O2 -I$(ROOT_DIR)
LIBS     := -lm
OPTIM    := -O3 -march=native -ffast-math

DOSSIER_OBJ := obj

# Exécutables cibles
CIBLE_TEST_NOISE := ./bin/zyn_test_noise
CIBLE_TEST_MAP   := ./bin/zyn_test_gen_map_macro

# Fichiers sources mutualisés
SRC_COMMUNS  := zyn_noise.c zyn_gen_map_relief.c zyn_gen_map_temperature.c zyn_gen_png.c
OBJ_COMMUNS  := $(SRC_COMMUNS:%.c=$(DOSSIER_OBJ)/%.o)

# Fichiers sources spécifiques aux tests
SRC_T_NOISE  := zyn_test_noise.c
SRC_T_MAP    := zyn_test_gen_map_relief.c

OBJ_T_NOISE  := $(DOSSIER_OBJ)/zyn_test_noise.o
OBJ_T_MAP    := $(DOSSIER_OBJ)/zyn_test_gen_map_macro.o

.PHONY: all release debug clean

all: release

release: CFLAGS += $(OPTIM)
release: $(CIBLE_TEST_NOISE) $(CIBLE_TEST_MAP)

debug: CFLAGS += -g -O0 -DDEBUG
debug: $(CIBLE_TEST_NOISE) $(CIBLE_TEST_MAP)

# Compilation de l'exécutable de test géomorphologique
$(CIBLE_TEST_MAP): $(OBJ_COMMUNS) $(OBJ_T_MAP)
	@echo "Linking $@..."
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)
	@echo "-> Générateur macro prêt : ./$(CIBLE_TEST_MAP)"

# Compilation de l'exécutable de test du bruit brut
$(CIBLE_TEST_NOISE): $(DOSSIER_OBJ)/zyn_noise.o $(OBJ_T_NOISE)
	@echo "Linking $@..."
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)
	@echo "-> Test du bruit prêt : ./$(CIBLE_TEST_NOISE)"

# Règle générique pour la compilation des objets (.c -> .o)
$(DOSSIER_OBJ)/%.o: %.c | $(DOSSIER_OBJ)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

$(DOSSIER_OBJ):
	@mkdir -p $(DOSSIER_OBJ)

clean:
	@echo "Nettoyage des fichiers objets et exécutables..."
	rm -rf $(DOSSIER_OBJ) $(CIBLE_TEST_NOISE) $(CIBLE_TEST_MAP)
	@echo "Nettoyage effectué."
