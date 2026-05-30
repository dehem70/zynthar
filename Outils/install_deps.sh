#!/bin/bash

# Arrêter le script en cas d'erreur
set -e

echo "=================================================="
echo "   Zynthar - Installation des Dépendances & Conf"
echo "=================================================="

# Vérification que le script est exécuté avec sudo
if [ "$EUID" -ne 0 ]; then
  echo "[-] Erreur : Ce script doit être exécuté en tant que root (utilisez sudo)."
  exit 1
fi

# 1. Détermination dynamique de la racine (Script situé dans /Outils/)
# On récupère le dossier absolu du script, puis on remonte d'un niveau via '..'
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ZYNTHAR_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"

# Alignement sur la nouvelle arborescence
DATA_DIR="$ZYNTHAR_ROOT/data"
REPORTS_DIR="$ZYNTHAR_ROOT/reports"

echo "[+] Dossier des outils détecté : $SCRIPT_DIR"
echo "[+] Racine du projet calculée  : $ZYNTHAR_ROOT"

echo "[+] Mise à jour des listes de paquets (apt update)..."
apt update -y

echo "[+] Installation des outils de compilation et dépendances système..."
apt install -y build-essential cmake libsqlite3-dev sqlite3

# 2. Configuration des variables d'environnement dans le .bashrc de l'utilisateur
if [ -n "$SUDO_USER" ]; then
    USER_HOME=$(eval echo "~$SUDO_USER")
    BASHRC_FILE="$USER_HOME/.bashrc"
    
    echo "[+] Configuration de l'environnement dans $BASHRC_FILE..."
    
    # Gestion de ZYNTHAR_ROOT (Regex et mise à jour)
    if grep -q "ZYNTHAR_ROOT" "$BASHRC_FILE"; then
        sed -i "s|export ZYNTHAR_ROOT=.*|export ZYNTHAR_ROOT=\"$ZYNTHAR_ROOT\"|g" "$BASHRC_FILE"
        echo "[*] Variable ZYNTHAR_ROOT mise à jour."
    else
        echo "" >> "$BASHRC_FILE"
        echo "# Configuration du projet Zynthar" >> "$BASHRC_FILE"
        echo "export ZYNTHAR_ROOT=\"$ZYNTHAR_ROOT\"" >> "$BASHRC_FILE"
        echo "[+] Variable ZYNTHAR_ROOT ajoutée."
    fi
    
    # Ajout du répertoire des outils dans le PATH (S'assure qu'il n'y est pas déjà)
    # On ajoute la variable relative $ZYNTHAR_ROOT/Outils pour rester portable
    ZYN_PATH_LINE='export PATH="$ZYNTHAR_ROOT/Outils:$PATH"'
    if grep -q "ZYNTHAR_ROOT/Outils" "$BASHRC_FILE"; then
        echo "[*] Le chemin vers les outils Zynthar est déjà présent dans le PATH."
    else
        echo "$ZYN_PATH_LINE" >> "$BASHRC_FILE"
        echo "[+] Répertoire des outils ajouté au PATH dans le .bashrc."
    fi
    
    # Ajustement des permissions pour que l'utilisateur reste propriétaire de son .bashrc
    chown "$SUDO_USER":"$SUDO_USER" "$BASHRC_FILE"
fi

# 3. Création préventive des répertoires d'exécution locaux
if [ ! -d "$DATA_DIR" ]; then
    mkdir -p "$DATA_DIR"
    echo "[+] Dossier de données local créé : $DATA_DIR"
fi

if [ ! -d "$REPORTS_DIR" ]; then
    mkdir -p "$REPORTS_DIR/maps"
    mkdir -p "$REPORTS_DIR/tests/determinism"
    mkdir -p "$REPORTS_DIR/benchmarks"
    echo "[+] Structure de rapports créée : $REPORTS_DIR"
fi

# Réassignation globale de la propriété des dossiers générés à l'utilisateur non-root
if [ -n "$SUDO_USER" ]; then
    chown -R "$SUDO_USER":"$SUDO_USER" "$DATA_DIR"
    chown -R "$SUDO_USER":"$SUDO_USER" "$REPORTS_DIR"
fi

echo "=================================================="
echo "[+] Configuration terminée avec succès !"
echo "💡 Rechargez votre terminal ou lancez : source ~/.bashrc"
echo "=================================================="
