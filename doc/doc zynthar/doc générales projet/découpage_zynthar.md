Voici la mise en forme Markdown complète et structurée de votre document de découpage architectural. J'ai réorganisé le texte pour le rendre hautement scannable, intégré des tableaux pour les sous-modules complexes (Web3, Maintenance) et nettoyé le diagramme ASCII d'origine pour un rendu visuel optimal.

# 🏗️ Architecture Modulaire de Zynthar

Ce document présente le découpage fonctionnel complet de l'univers de Zynthar, divisé en 7 briques techniques interdépendantes.

## 🚧 Brique 1 : Le Serveur Central (C Pur)

Le serveur est le cœur du jeu, le « chef d'orchestre » qui détient la vérité absolue sur l'état du monde.

- **La Boucle Principale (Tickrate) :** Implémentation d'une boucle à fréquence fixe cadencée à 20 Hz (soit 50 ms par tick) gérant l'ensemble de la physique et de la logique.
    
- **Le Gestionnaire de Concurrence :** Une file d'attente (_Message Queue_) pour traiter les actions des joueurs une par une selon le principe du premier arrivé, premier servi, évitant tout conflit sur un même voxel.
    
- **Le Système de Collision / Mouvement :** Intégration de la logique de déplacement basée sur les règles métiers (gestion des obstacles de 40 cm, 1 m, 1,6 m et maintien d'une distance minimale de 1 m entre deux joueurs).
    

## 🗄️ Brique 2 : Le Moteur de Persistance (C + SQLite3)

Cette brique est responsable de charger le monde et de sauvegarder les actions des joueurs sans jamais bloquer le serveur central.

- **Couche d'abstraction SQLite :** Code en C conçu pour interroger la base des 2 millions de macro-chunks de manière asynchrone ou ultra-rapide grâce à un index composite.
    
- **Générateur Procédural Déterministe :** Intégration des algorithmes de bruit mathématique en C et de la logique de peuplement des biomes.
    
- **Gestionnaire de Delta (Micro-chunks) :** Système qui enregistre uniquement les blocs posés ou détruits par les joueurs dans une table secondaire afin de ne pas alourdir la génération de base.
    

## 🌐 Brique 3 : La Couche Réseau Hybride (C / JS)

La plomberie fine pour garantir un transfert de données parfait sans latence perçue par l'utilisateur.

- **Moteur d'AoI (Area of Interest) :** Algorithme calculant dynamiquement la « bulle » de chaque joueur pour ne lui envoyer que les chunks visibles dans son rayon d'action.
    
- **Pipeline de Sérialisation (FlatBuffers / RLE) :** Code de compression à la volée en C pour empaqueter les voxels au format binaire brut, et décompression en TypeScript/JavaScript côté client.
    
- **Gestionnaire de Protocoles :** Routage des données asynchrones.
    
    - _WebSockets :_ Dédié aux actions critiques (ex: modification de blocs).
        
    - _WebRTC / WebTransport :_ Dédié à la transmission fluide des positions $X, Y, Z$ des joueurs.
        

## 🎮 Brique 4 : Le Client 3D (TypeScript / Babylon.js / WebGPU)

Le moteur graphique qui s'exécute directement dans le navigateur de l'utilisateur, optimisé pour les Chromebooks via une architecture PWA.

- **Système de Voxel Streaming :** Logique qui demande et reçoit les paquets binaires du serveur en tâche de fond pour assembler les maillages 3D (_Meshing_).
    
- **Rendu WebGPU :** Pipeline d'affichage ultra-performant tirant parti de la carte graphique pour afficher de grands volumes de voxels sans baisse de framerate.
    
- **Contrôles Première Personne & Interaction :** Gestion de la caméra, de la physique locale (pré-calcul des collisions) et tracé de rayon (_Raycasting_) pour matérialiser la sphère d'interaction (ex: portée limite de 3 mètres pour modifier un bloc).
    

## 🛠️ Brique 5 : Les Outils de Génération Procédurale Offline (C)

Cette brique permet d'initialiser l'univers du jeu en amont.

- **Le Générateur de Squelette (C) :** Script utilisant des algorithmes de bruit (Perlin/Simplex) pour pré-calculer et remplir la base SQLite des 2 millions de macro-chunks avec les métadonnées de biomes, températures et humidités.
    

## 🪙 Brique 6 : Écosystème Web3 & Économie en jeu

Cette brique gère la confiance, la propriété des actifs (items, parcelles de terrain) et la passerelle financière (entrées/sorties de fonds).

|**Composant**|**Rôle Technique**|**Fonctionnement dans Zynthar**|
|---|---|---|
|1. Smart Contracts _(MultiversX / Massa)_|Code immuable sur la blockchain.|- **Contrat "gain" :** Gestion des frais d'inscription ou des pools de récompenses en EGLD .<br><br>  <br><br>- **Gestion d'actifs :** Standards natifs ESDT (SFT/NFT) pour les parcelles/items sans surcoût en gas .<br><br>  <br><br>- **Cross-Chain :** Logique de pont (Bridge) vers Massa.|
|2. Relais Serveur-Blockchain _(Oracle interne en C)_|Composant intermédiaire indispensable car le serveur de jeu ne peut pas attendre la validation d'un bloc (6s).|- **Event Listener :** Démon (Go/Python/C) qui surveille la blockchain et met à jour l'inventaire en mémoire vive lors d'un achat ou dépôt .<br><br>  <br><br>- **Validateur de Possession :** Vérifie la signature Web3 avant d'accorder des droits de modification (ex: zone protégée).|
|3. Intégration Client _(Portefeuille Web3)_|Interface utilisateur dans l'application Babylon.js / PWA.|- **Login Web3 :** Connexion via xPortal (QR Code), DeFi Wallet ou Ledger (l'adresse devient l'ID unique dans SQLite) .<br><br>  <br><br>- **Signature :** Interface fluide pour signer des actions économiques en jeu.|

## 🧰 Brique 7 : Outillage, Maintenance & Tests (Python / CLI)

Regroupe tous les programmes « hors-jeu » servant à valider, réparer, surveiller et faire évoluer le monde de Zynthar.

1. Utilitaires de Maintenance de la Base de Données (SQLite3)

- **Inspecteur / Extracteur de Chunk :** Outil CLI pour exporter les données d'un chunk au format texte (JSON ou CSV) afin de vérifier la cohérence des variables (température, humidité, élévation).
    
- **Nettoyeur de Deltas (Garbage Collector) :** Analyse la table des deltas pour supprimer les actions redondantes (ex: poses et destructions successives par un joueur) et exécute un `VACUUM` pour optimiser l'espace disque.
    
- **Outil de Migration de Schéma :** Permet de mettre à jour le schéma des 2 millions de lignes sans perte de données en cas d'ajout de nouvelles variables d'environnement (ex: pression atmosphérique, vent).
    

2. Suite de Validation Algorithmique (Tests de Non-Régression)

- **Validateur de Déterminisme Cross-Language :** Script crucial qui génère 10 000 chunks à la fois en C (serveur) et en TypeScript (client) puis compare les fichiers binaires au bit près pour s'assurer de l'absence totale de dérive mathématique (évite que les joueurs lévitent ou s'enfoncent).
    
- **Générateur de Profil de Monde :** Script Python qui extrait les 2 millions de lignes de macro-chunks pour générer une image PNG haute résolution (carte thermique globale) afin d'auditer visuellement la distribution des biomes.
    

3. Outils de Simulation Réseau et Charge (Stress Testing)

- **Simulateur de Bots :** Programme émulant 100 clients virtuels se déplaçant et modifiant des blocs en boucle afin de valider la tenue du tickrate de 20 Hz du serveur en C sous charge maximale sans verrous bloquants sur SQLite.
    

## 📊 Diagramme d'Architecture Échange de Données

```
                       ┌────────────────────────────────┐
                       │       6. ÉCOSYSTÈME WEB3       │
                       │  (Contrats MultiversX, xPortal)│
                       └───────────────┬────────────────┘
                                       │ (Vérification / Transactions)
                                       ▼
┌────────────────────────┐    ┌────────────────────┐    ┌─────────────────────────┐
│      4. CLIENT 3D      │    │  3. COUCHE RÉSEAU  │    │   1. SERVEUR CENTRAL    │
│ (Babylon.js / WebGPU)  │◄──►│  (FlatBuffers/RLE) │◄──►│         (C Pur)         │
└────────────────────────┘    └────────────────────┘    └───────────┬─────────────┘
                                                                    │
                                                                    ▼
┌────────────────────────┐                              ┌─────────────────────────┐
│5. OUTILS DE GÉNÉRATION │                              │ 2. MOTEUR DE PERSISTANCE│
│  (Bruit Déterministe)  │                              │(SQLite3: Macro + Deltas)│
└────────────────────────┘                              └─────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────┐
│                        7. OUTILS DE MAINTENANCE ET TESTS                        │
└─────────────────────────────────────────────────────────────────────────────────┘
```