🚧 Brique 1 : Le Serveur Central (C Pur)
C'est le cœur temporel et l'autorité physique absolue de notre v0.1.  ￼

Tâche 1.1 : Structure de la boucle principale (Tickrate) – Implémenter l'horloge fixe à 20 Hz (1 tick toutes les 50 ms) avec des timers haute résolution pour éviter toute dérive.  ￼

Tâche 1.3 : Gestionnaire d'état des joueurs – Créer la structure Player en C (ID, position exacte X,Y,Z, vitesse, chunk actuel, etc.).  ￼

Tâche 1.4 : Moteur de collision Voxel/AABB – Développer l'algorithme de collision entre la Bounding Box du joueur et les micro-voxels environnants.  ￼

Tâche 1.5 : Implémentation des règles métiers de mouvement – Coder le système d'enjambement automatique pour les obstacles ≤40 cm (marche fluide) et les règles de blocage pour franchir les paliers supérieurs par le saut ou l'escalade.  ￼

🗄️ Brique 2 : Le Moteur de Persistance & Génération Locale (C + SQLite3)
Ici, on initialise le squelette du monde et on sculpte le terrain au format brut.  ￼

Tâche 2.1 : Binding et initialisation de SQLite3 en C – Configurer la base avec les PRAGMA de performance (WAL, synchronous=NORMAL).  ￼

Tâche 2.2 : Couche d'accès Macro-Chunks – Permettre au serveur de récupérer instantanément les métadonnées d'un macro-chunk via sa clé primaire composite (chunk_x, chunk_y).  ￼

Tâche 2.3 : Intégration des fonctions de Bruit Mathématique – Implémenter le bruit de gradient déterministe (Perlin/Simplex) en C pur avec notre seed unique.  ￼

Tâche 2.4 : Décorateur de Biomes – Traduire les valeurs de bruit lues en blocs de voxels (ex: transformer le biome désert en sable).  ￼

🌐 Brique 3 : La Couche Réseau Hybride (C / JS)
L'objectif est d'assurer un streaming binaire ultra-léger (zéro JSON) du serveur vers le client.  ￼

Tâche 3.1 : Serveur WebSockets en C – Ouvrir le tunnel réseau asynchrone pour streamer les données.  ￼

Tâche 3.3 : Compresseur RLE (Run-Length Encoding) – Coder la fonction en C qui compresse à la volée les micro-chunks (32×32×32 blocs) pour réduire la taille des paquets de 90% à 95%.  ￼

Tâche 3.4 : Schémas de sérialisation FlatBuffers – Définir le schéma .fbs (PositionJoueur, DemandeChunk) et compiler pour générer le code C et TypeScript.  ￼

🎮 Brique 4 : Le Client 3D (TypeScript / Babylon.js)
Donner vie au rendu visuel sur le Chromebook sans saturer la RAM ni faire trembler l'affichage.  

Tâche 4.1 : Architecture PWA & Scène Babylon.js – Initialiser le projet avec caméra à la première personne optimisée pour Chromebook.  ￼

Tâche 4.2 : Module de décompression et maillage des Voxels (Meshing) – Écrire le décompresseur RLE en TypeScript et implémenter le Greedy Meshing pour fusionner les faces de voxels identiques selon le niveau de LOD et minimiser les polygones.  ￼

Tâche 4.3 : Client-Side Prediction & Tâche 4.4 : Algorithme de Réconciliation – Indispensables pour que la caméra locale réagisse instantanément au clavier tout en se calant sur l'autorité binaire du serveur.  ￼

Tâche 4.X (Spécification LOD issue de la Roadmap Étape 2 & 4) – Coder la logique des LOD radiaux (LOD 0 à 10 cm jusqu'à 25 m, LOD 1 à 40 cm jusqu'à 100 m, et LOD 2 à 1,6 m jusqu'à 400 m) en utilisant des décalages de bits sur le CPU.  ￼

Tâche 4.X (Spécification Origine Flottante issue de la Roadmap Étape 4) – Recentrer cycliquement la scène Babylon.js (0,0,0) sur le micro-chunk du joueur pour éliminer le sautillement des vertices lointains.  ￼

🛠️ Brique 5 : Les Outils de Génération Procédurale Offline (C)
C'est le pipeline "offline" pour pré-calculer le squelette de notre monde de 1000×500 km.  ￼

Tâche 5.1 : Script de découpage géométrique – Boucler sur la grille complète des 2 millions de macro-chunks (2000×1000).  ￼

Tâche 5.2 : Générateur macro-procédural – Calculer les zones climatiques (température, humidité) et reliefs via les bruits superposés.  ￼

Tâche 5.3 : Injecteur de masse SQLite (Bulk Insert) – Injecter les 2 millions de lignes en moins de 10 secondes grâce aux transactions massives (BEGIN / COMMIT).  ￼

🧰 Brique 7 : Outillage, Maintenance & Tests (Python / CLI)
Pour valider nos mathématiques et notre rendu avant d'aller plus loin.  ￼

Tâche 7.2 : Test de cohérence du déterminisme (Cross-Language) – Cruciale. Générer un échantillon de 1 000 chunks en C et en TypeScript et comparer les fichiers au bit près pour s'assurer que le joueur ne lévite pas ou ne s'enfonce pas dans le sol à cause d'un décalage mathématique.  ￼

Tâche 7.3 : Cartographe 2D du monde – Développer le script Python pour extraire les 2 millions de macro-chunks et générer la carte globale en haute résolution pour notre audit visuel.  ￼
