# 📑 Spécification d'Architecture — Brique 2 : Persistance & Génération Locale (V2.1)

La Brique 2 est un sous-système de persistance et de génération procédurale purement réactif, isolé des règles métiers. Elle n'a aucune autorité de décision et agit comme un fournisseur de données en tâche de fond pour la Brique 1 (Serveur Central) et la Brique 3 (Réseau).

## 1. Topologie des Échanges et Flux d'Autorité

- **Lecture / Streaming :** La Brique 3 (Réseau) ou la Brique 1 demande un Micro-Chunk ($25.6\text{ m}$). La Brique 2 le génère via sa chaîne de workers, le compresse en RLE et le renvoie.
    
- **Écriture / Validation de Blocs :** La Brique 1 valide l'action physique et métier du joueur (dans son tick à 20 Hz). Une fois l'action confirmée et envoyée au client, la Brique 1 notifie la Brique 2 de la modification. La Brique 2 met immédiatement à jour son cache RAM (LOD 0) et planifie l'écriture SQL asynchrone.
    

## 2. Découpage Géométrique et Alignement Cache CPU

- **Le Micro-Chunk (Vue Transport/Réseau) :** Représente une matrice de $256 \times 256 \times 256$ voxels (`uint8_t`), soit l'unité idéale de streaming binaire pour ne pas multiplier les en-têtes de paquets réseau.
    
- **Le Nano-Chunk (Unité de Calcul L1/L2) :** L'espace de travail est fragmenté géométriquement en blocs de **$16 \times 16 \times 16$ octets (4 Ko)**. Cette taille correspond à une page mémoire matérielle et réside intégralement dans le cache L1 d'un cœur CPU, garantissant une vitesse de traitement mathématique maximale sans solliciter le bus RAM.
    
- **Arènes de Mémoire Fixes :** Toutes les structures (Contextes, Nano-Chunks) sont pré-allouées dans des _Memory Pools_ continus au démarrage. L'usage de `malloc`/`free` en runtime est banni.
    

## 3. Architecture des Workers Découplés

On utilise un modèle de **Données Indépendantes** et d'**I/O isolée** pour éviter le _Cache Line Bouncing_ (ping-pong entre cœurs CPU).

### ### 🧩 Worker 0 : Centralisateur I/O, Gardien SQLite3 & Interface Réseau (B3)

* **Canal de Communication Réseau (B3 $\rightarrow$ B2) :** Le Worker 0 est le point d'entrée et de sortie unique pour toutes les communications externes de la Brique 2. Il maintient une interface réseau asynchrone bidirectionnelle avec la Brique 3 (Réseau) via un flux TCP brut standardisé au format FlatBuffers. Ce flux réseau transite sans aucune désérialisation textuelle (zéro JSON) pour éliminer l'overhead CPU et la surconsommation de mémoire vive.
* **Gestion du Flux Entrant (Streaming & Écritures) :** En entrée, le Worker 0 écoute les demandes de streaming de Micro-Chunks initiées par les déplacements des utilisateurs, ainsi que les notifications de modifications de blocs (Deltas) validées en amont par l'autorité physique de la Brique 1. 
* **Gestion du Flux Sortant (Validation & Rendu) :** En sortie, dès qu'une modification locale est appliquée dans le cache RAM ou qu'un nouveau Micro-Chunk est finalisé par le Worker 2 (Compresseur RLE), le Worker 0 injecte immédiatement le paquet binaire résultant dans le flux réseau à destination de la Brique 3. Celle-ci se charge alors de le distribuer au client émetteur pour confirmation graphique locale, ou de le diffuser aux joueurs limitrophes via le calcul de la Bulle d'Intérêt (AoI).

- **Rôle :** Il détient l'unique connexion active en lecture/écriture vers le `ramdisk`.
    
- **Mécanique de Lecture (_Staging_ séquentiel) :** Au démarrage, il exécute un `ATTACH DATABASE` pour unifier l'espace de nommage des bases (`main` pour le relief, `rivers` pour les cours d'eau, `deltas` pour les modifications, ...). Pour charger un chunk, il exécute **x requêtes préparées simples et successives** via son unique thread, effectuant des copies binaires directes (_casts_ sauvages) des `BLOB` sans faire de `UNION` ou `JOIN` SQL coûteux.
    
- **Mécanique d'Écriture (Bulk Transaction) :** Les demandes de modification de blocs envoyées par la Brique 1 sont stockées dans une `Write Queue` en RAM. Le Worker 0 les applique immédiatement dans le cache brut de la RAM, puis vide la queue toutes les secondes dans la base `deltas.db` via une transaction unique (`BEGIN` / `COMMIT`).
    

### ✂️ Worker 1 : L'Ordonnanceur (Splitter)

- **Rôle :** Il reçoit le contexte chargé par le Worker 0 et le fragmente en $4096$ NanoJobs de $16^3$ voxels.
    
- **Gestion des structures inter-chunks :** C'est ici que sont gérés les grands objets (ex: maisons de 200 voxels). Le Worker 1 effectue un test d'intersection mathématique rapide à base d'AABB (Boîtes Englobantes). Si une structure traverse le nano-chunk, sa référence de patron (_Blueprint_) et son décalage local exact ($X,Y,Z$) sont injectés dans le NanoJob.
    

### 🧮 Pool de X Workers de Calcul (Pool de Threads)

- **Rôle & Dimensionnement :** Ce pool est constitué de `X` threads identiques et autonomes, où `X` s'aligne strictement sur le nombre de cœurs CPU physiques du serveur pour saturer la puissance de calcul sans hyperthreading délétère.
    
- **Mécanique d'Acquisition (Pool de Blocs Libres) :** Lorsqu'un thread pioche un `NanoJob` dans le Ring Buffer atomique (`stdatomic.h`), il interroge immédiatement un _Pool de Blocs Libres_ pré-alloué au démarrage du serveur (Arène mémoire fixe, exclusion de tout `malloc` en runtime). Il y récupère l'adresse RAM d'un slot vierge de $4\text{ Ko}$ ($16 \times 16 \times 16$ octets).
    
- **Exécution Isolée en Cache CPU (L1/L2) :** Le thread exécute la totalité des algorithmes procéduraux (bruits de Perlin/Simplex, décorateur de biome, rasterisation des patrons d'AABB et fusion des deltas) en écrivant directement à cette adresse. Le matériel (CPU) intercepte ces opérations et monte instantanément le bloc de $4\text{ Ko}$ dans son **Cache L1/L2**. Le thread travaille ainsi à la vitesse théorique maximale du silicium ($> 1\text{ To/s}$), totalement isolé des autres cœurs et sans jamais polluer ou solliciter la RAM principale.
    
- **Enregistrement des Résultats (Zéro-Copy & Write-Back) :** À la fin de sa tâche, le thread n'effectue aucun transfert de données physique. Il écrit simplement l'adresse mémoire du bloc ($8$ octets) dans la table d'indexation topologique du `MicroChunkContext` (géré par le Worker 1). Cette écriture se fait par indexation vectorielle directe (`index = x + (y << 4) + (z << 8)`), garantissant une exécution _Lock-Free_ (zéro mutex).
    
- **Enchaînement Immédiat :** Dès le pointeur enregistré, le thread abandonne la propriété du bloc et retourne instantanément piocher un nouveau `NanoJob` pour un autre secteur. La donnée brute du nano-chunk reste "chaude" dans les lignes de cache du processeur (politique matérielle de _Write-Back_), prête à être consommée directement depuis le cache CPU par le **Worker 2** pour la compression RLE finale.
    

### 🗜️ Worker 2 : Le Compresseur RLE Élite

- **Rôle :** Il récupère le tableau de pointeurs ordonnés des 4096 nano-chunks complétés par le pool de calcul.
    
- **Fonction :** Il parcourt de manière strictement linéaire les structures de 4 Ko, activant le _Hardware Prefetching_ du CPU. L'algorithme RLE compresse les données à la volée (90% à 95% de gain de taille). Le flux binaire final est stocké en cache et transmis au Worker 0 pour envoi réseau.

### 🛡️ Le Surveillant Indépendant (Watchdog & Orchestrateur)

Le Surveillant est un composant autonome s'exécutant de manière isolée pour piloter la résilience, la persistance physique et l'élasticité de la Brique 2 sans jamais interférer avec la boucle logique des Workers.

- **Initialisation du Système (Bootstrapping) :** Au lancement de la Brique 2, le Surveillant prend le contrôle exclusif du cycle de démarrage. Il exécute les appels système nécessaires pour monter le `ramdisk` en mémoire vive, puis effectue la copie à chaud des fichiers SQLite3 physiques (`zyn_world.db`, `zyn_rivers.db`, `zyn_deltas.db`...etc ) depuis le stockage persistant (SSD/NVMe) vers ce `ramdisk` avant de lancer les différents workers .
    
- **Stratégie de Sauvegarde Synchrone (Flush To Disk) :** Afin de prémunir le serveur d'une perte de données fatale en cas de coupure de courant ou de crash système, le Surveillant intègre une routine de synchronisation. À une fréquence strictement définie par un fichier de configuration externe (`config.json` ou `config.ini`), il effectue un snapshot ou une copie de sauvegarde asynchrone du fichier `zyn_deltas.db` depuis le `ramdisk` éphémère vers le disque dur persistant.
    
- **Détection des Blocages et Récupération des Jobs :** En plus de surveiller les signaux système (crashs), le Surveillant inspecte la file des NanoJobs à intervalles réguliers. Si un job reste à l'état `0x01` (EN COURS) pendant un nombre anormalement élevé de ticks de la boucle centrale, le Surveillant consulte le `worker_id` associé.
    
- **Procédure de Sauvetage :** 1. Le Surveillant utilise l'appel système approprié pour **tuer le thread/processus worker** identifié comme défaillant ou gelé. 2. Il instancie immédiatement un nouveau thread sain dans la pool pour maintenir la capacité de calcul. 3. **Il réinitialise atomiquement l'état du NanoJob en souffrance à `0x00` (À FAIRE)** et remet le `worker_id` à vide.
    
- **Auto-Scaling Dynamique (Élasticité de la Pool) :** Le Surveillant agit comme le régulateur de charge du CPU. Il inspecte cycliquement la longueur de la file d'attente des `NanoJobs` (Ring Buffer atomique). Si la file d'attente sature (forte concentration de joueurs se déplaçant simultanément), il engendre dynamiquement de nouveaux Workers de calcul dans la pool (jusqu'à la limite des cœurs physiques disponibles). À l'inverse, lorsque la charge redescend, il ordonne aux Workers excédentaires de se terminer proprement afin de restituer les ressources CPU à la Brique 1 (Serveur Central) pour ses calculs physiques et métiers.

## 4. Stratégie du Cache Multi-LOD

Le cache utilise une table de hachage globale partitionnée (sous-verrous atomiques) indexée sur un entier unique `uint64_t` :

`Key = Macro_ID (32 bits) | mc_x (8 bits) | mc_y (8 bits) | mc_z (8 bits) | LOD (8 bits)`

### Cycle de vie des données en Cache :

1. **LOD 0 (Bulle active, Rayon < 25.6m) :** Stocké sous forme **brute** (Tableau de pointeurs vers Nano-Chunks de 4 Ko). Cela permet au Worker 0 d'appliquer les modifications de blocs des joueurs directement en RAM en moins d'une microseconde, sans recalculer le chunk.
    
2. **LOD 1 & 2 (Décor lointain, Horizon jusqu'à 400m) :** Stocké sous forme **compressée RLE**. L'empreinte RAM est minime, permettant de maintenir l'horizon de 100 joueurs simultanés sans saturation.
    
3. **Optimisation par Up-Sampling :** Si un client demande un chunk en LOD 1 (moyenne portée) et que le LOD 0 est déjà présent en cache, le Worker 1 extrait directement les données par échantillonnage déterministe (pas de 4 blocs), court-circuitant l'intégralité du pool de calcul mathématique.
    
4. **Éviction :** Gérée par le Worker 2 via un algorithme LRU (_Least Recently Used_) réutilisant les emplacements des arènes mémoires fixes.

### 📊 Cycle de Vie et Machine d'États des NanoJobs

Pour assurer qu'aucune tâche de calcul ne soit perdue lors d'une défaillance d'un thread, la file d'attente globale (Ring Buffer) n'est pas une simple liste de lecture, mais une table d'états atomique supervisée. Chaque `NanoJob` est tagué par un octet d'état (`uint8_t job_state`) et un identifiant de thread (`uint8_t worker_id`).

|**Valeur de job_state**|**Désignation**|**Signification Technique**|
|---|---|---|
|`0x00`|**À FAIRE (A_FAIRE)**|Le job est prêt dans la file, en attente d'être pioché par un thread disponible.|
|`0x01`|**EN COURS (EN_COURS)**|Le job a été pris. Le champ `worker_id` contient l'ID du thread actif.|
|`0x02`|**TERMINÉ (TERMINE)**|Le thread a écrit son pointeur dans le contexte. Le job est extrait de la boucle de surveillance.|