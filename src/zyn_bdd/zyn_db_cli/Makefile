# Variables de compilation
CC = gcc
# Récupère le chemin absolu du dossier où se trouve ce Makefile
ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

# Tu peux ensuite t'en servir pour définir des dossiers de build ou d'include cleanly
CFLAGS = -Wall -Wextra -O2 -I$(ROOT_DIR)
LIBS = -lsqlite3

# Nom de l'exécutable final
TARGET = ./bin/zyn_db_cli

# Source et objets
SRC = zyn_db_cli.c
OBJ = $(SRC:.c=.o)

# Règle par défaut (celle exécutée quand on tape juste 'make')
all: $(TARGET)

# Liaison de l'exécutable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LIBS)

# Compilation des fichiers objets
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Règle pour nettoyer les fichiers générés
clean:
	rm -f $(OBJ) $(TARGET)

# Règle pour forcer une recompilation propre
re: clean all

# Indique à make que ces règles ne correspondent pas à des fichiers physiques
.PHONY: all clean re
