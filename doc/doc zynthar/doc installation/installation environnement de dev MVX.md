
# 🚀 Guide d'Installation de l'Environnement MultiversX & Déploiement

Ce guide technique détaille la configuration de l'environnement de développement, la création, la compilation et le déploiement d'un Smart Contract sur le réseau MultiversX.

## 🛠️ Étape 1 : Installation de `mxpy` et de Rust

Pour compiler et déployer des contrats sur MultiversX, vous devez installer l'outil CLI officiel `mxpy` (via Python/pipx) ainsi que la chaîne de compilation Rust.

### 1. Installation des dépendances Python

Bash

```
# Installer pip (si ce n'est pas déjà fait)
sudo apt install python3-pip

# Installer pipx
sudo apt install pipx
```

### 2. Installation et vérification de `mxpy`

Bash

```
# Installer mxpy via pipx
pipx install mxpy

# Vérifier que l'installation a réussi
mxpy --version
```

> 💡 **Astuce d'ergonomie :** Si vous souhaitez activer l'autocomplétion des commandes `mxpy` directement dans votre terminal Linux, exécutez la commande suivante:
> 
> Bash
> 
> ```
> mxpy --install-shell-autocomplete
> ```

### 3. Installation de Rust

Rust est indispensable pour compiler le code source des contrats en byte-code WebAssembly (`.wasm`).

Bash

```
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

## 📐 Étape 2 : Génération de la trame du Smart Contract

MultiversX fournit l'outil `sc-meta` pour initialiser rapidement des projets à partir de modèles (_templates_).

Bash

```
# Générer un contrat vide nommé "mon-contrat"
sc-meta new --template empty --name mon-contrat
```

## ⚙️ Étape 3 : Modification et Compilation

### 1. Modification du code source

Avant de compiler, vous devez écrire ou modifier la logique métier de votre contrat dans le fichier source principal :

- 📂 **Fichier à éditer :** `mon-contrat/src/mon-contrat.rs` _(ou le fichier `.rs` principal généré dans le sous-répertoire `/src`)_.
    

### 2. Compilation du contrat

La compilation traduit le code Rust en un fichier exécutable par la machine virtuelle MultiversX et génère son interface de programmation (ABI).

Bash

```
# Se placer dans le dossier de votre projet
cd mon-contrat

# Compiler le contrat (génère les fichiers .wasm et l'ABI)
sc-meta all build
```

## 🌐 Étape 4 : Déploiement et Mises à jour

Une fois le contrat compilé, deux approches s'offrent à vous pour l'envoyer sur le réseau (ex: le Devnet).

### 🔐 Option A : Déploiement classique via `mxpy`

Cette méthode utilise une commande directe en ligne de commande en spécifiant le fichier `.wasm`, votre clé privée (`.pem`), la limite de Gas et le proxy du réseau.

Bash

```
mxpy contract deploy \
  --bytecode output/mon-contrat.wasm \
  --recall-nonce \
  --pem=votre_wallet.pem \
  --gas-limit=60000000 \
  --send \
  --proxy=https://devnet-gateway.multiversx.com \
  --chain=D
```

### 🦀 Option B : Déploiement via les _Interactors_ Rust (Recommandé)

Cette méthode moderne permet de générer un script en Rust pour automatiser, tester et reproduire facilement les futurs déploiements ou mises à jour de votre contrat.

Bash

```
# 1. Générer le dossier "interactor" depuis la racine de votre contrat
sc-meta all interactors

# 2. Exécuter le déploiement scripté via Cargo
cargo run deploy
```