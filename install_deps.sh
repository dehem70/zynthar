#!/bin/bash

# Arrêter le script en cas d'erreur
set -e

echo "=================================================="
echo "   Zynthar - Installation des Dépendances"
echo "=================================================="

# Vérification que le script est exécuté avec sudo
if [ "$EUID" -ne 0 ]; then
  echo "[-] Erreur : Ce script doit être exécuté en tant que root (utilisez sudo)."
  exit 1
fi

# 2. Détermination de la racine du projet
# On récupère le dossier où se trouve ce script
ZYNTHAR_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DB_DIR="./zyn_db"

echo "[+] Racine du projet détectée : $ZYNTHAR_ROOT"


echo "[+] Mise à jour des listes de paquets (apt update)..."
apt update -y

echo "[+] Installation des outils de compilation pour les programmes en C..."
# build-essential fournit gcc, make, etc.
# libsqlite3-dev fournit les headers (sqlite3.h) pour la compilation en C
apt install -y build-essential libsqlite3-dev sqlite3

# 4. Configuration de la variable d'environnement persistante
# On cherche le fichier .bashrc de l'utilisateur qui a lancé le sudo
if [ -n "$SUDO_USER" ]; then
    USER_HOME=$(eval echo "~$SUDO_USER")
    BASHRC_FILE="$USER_HOME/.bashrc"
    
    echo "[+] Configuration de ZYNTHAR_ROOT dans $BASHRC_FILE..."
    
    # On vérifie si la variable est déjà présente pour éviter les doublons
    if grep -q "ZYNTHAR_ROOT" "$BASHRC_FILE"; then
        # Si elle existe, on met à jour le chemin avec du regex (sed)
        sed -i "s|export ZYNTHAR_ROOT=.*|export ZYNTHAR_ROOT=\"$ZYNTHAR_ROOT\"|g" "$BASHRC_FILE"
        echo "[*] Variable ZYNTHAR_ROOT mise à jour."
    else
        # Si elle n'existe pas, on l'ajoute à la fin du fichier
        echo "" >> "$BASHRC_FILE"
        echo "# Configuration du projet Zynthar" >> "$BASHRC_FILE"
        echo "export ZYNTHAR_ROOT=\"$ZYNTHAR_ROOT\"" >> "$BASHRC_FILE"
        echo "[+] Variable ZYNTHAR_ROOT ajoutée au .bashrc."
    fi
    
    # Ajustement des permissions pour que l'utilisateur reste propriétaire de son .bashrc
    chown "$SUDO_USER":"$SUDO_USER" "$BASHRC_FILE"
fi

# 5. Création préventive du dossier db à la racine du projet
if [ ! -d "$DB_DIR" ]; then
    mkdir -p "$DB_DIR"
    # On redonne la propriété à l'utilisateur non-root
    if [ -n "$SUDO_USER" ]; then
        chown -R "$SUDO_USER":"$SUDO_USER" "$DB_DIR"
    fi
    echo "[+] Dossier '$DB_DIR' créé."
fi


echo "=================================================="
echo "[+] Toutes les dépendances ont été installées !"
echo "=================================================="
