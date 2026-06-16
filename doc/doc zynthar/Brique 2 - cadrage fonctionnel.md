La Brique 2 est l'usine mathématique et volumétrique du serveur. Son rôle exclusif est de transformer les requêtes géographiques du réseau en paquets de voxels sculptés, compressés et prêts à l'expédition. Elle repose sur une architecture de processus découplés communiquant via de la mémoire partagée (SHM) et des fichiers tampons (FIFOs).

## 1. INFRASTRUCTURE ET SYSTÈME

### 🧠 1.1 La Mémoire Partagée (SHM)

La SHM est organisée en un pool fixe de **Postes de Travail** (ou Châssis). Chaque poste possède une structure C stricte contenant :

- **L'En-tête :** `Job_ID`, `Macro_Chunk_ID`, Coordonnées locales ($X, Y, Z$) du Meso-Chunk, et l'état global du Job.
    
- **La Carte d'Identité du Meso-Chunk :** Contient la fiche géologique locale (9 points de contrôle, amplitudes, biome principal) et un tableau de recensement des biomes voisins (`uint8_t neighbor_biomes[4]`).
    
- **Le Tableau des 64 Micro-Chunks :** Pour chaque bloc de 4m, un flag d'état (`UNIFORME` ou `A_FORGER` / `FORGE`), un octet `MATIERE_ID`, un masque binaire `uint64_t material_mask`, et les points géométriques locaux d'intersection de frontière (`intersection_start`, `intersection_end`).
    
- **Le Buffer Volumétrique :** L'espace brut où les forgerons sculptent les voxels si le bloc est complexe.
    
- **La Zone de Paquet Réseau :** L'espace mémoire final où le flux compressé est écrit pour Chronos.
    

### 🔀 1.2 La Colonne Vertébrale (Les FIFOs)

Les processus ne se connaissent pas et dorment sur des files d'attente système :

- `FIFO_FREE_STATIONS` : Réserve des numéros de postes de travail libres et garantis propres (remplis de `0` / Air).
    
- `FIFO_A_NETTOYER` : Tampon de transit pour les postes usagés en attente de désinfection.
    
- `FIFO_REQUESTED` : Entrée de la chaîne (Alimentée par Chronos $\to$ Lue par Hécate).
    
- `FIFO_ANALYSED` : File des Meso-Chunks complexes (Alimentée par Hécate $\to$ Lue par Atropos).
    
- `FIFO_MICRO_A_FORGER` : Sous-file de l'atelier de forge (Alimentée par Atropos $\to$ Lue par les Forgerons).
    
- `FIFO_RETOUR_FORGERONS` : Comptes-rendus des ouvriers (Alimentée par les Forgerons $\to$ Lue par Atropos).
    
- `FIFO_FORGED` : File des Meso-Chunks sculptés (Alimentée par Atropos $\to$ Lue par Atlas).
    
- `ZYN_STATE_COMPRESSED` : Sortie de la chaîne (Alimentée par Atlas/Hécate $\to$ Lue par Chronos).
    

## 👥 2. LES PROFILS DES TRAVAILLEURS

### 🛡️ 2.1 Cerbère (L'Intendant de la Mémoire et Tuteur Système)

- **`fifo_in_nettoyage` :** `FIFO_A_NETTOYER` (Récupère les index des tables usagées à désinfecter)
    
- **`fifo_out_dispo` :** `FIFO_FREE_STATIONS` (Propulse les index des tables propres prêtes pour un nouveau job)
    
- _Note :_ Il possède également un accès direct aux API Linux (`fork`, `waitpid`, `shmget`/`munmap`) pour la surveillance des PID et l'élasticité de la SHM.

Cerbère est le gardien des ressources. Il n'intervient jamais dans le flux de production direct.

- **Gestion des processus :** Il `fork()` et surveille la vie de tous les autres processus (Chronos, Hécate, Atropos, Atlas, et le pool de Forgerons). En cas de crash, il abat le processus défaillant et en relance un propre en moins de 500 ms.
    
- **L'Élasticité Mémoire (Garbage Collecting) :** Il observe la balance entre deux FIFOs :
    
    - _Boucle nominale :_ Il dépile la `FIFO_A_NETTOYER`, applique un `memset` à `0` (Air pur) sur la zone voxel du poste, et injecte le numéro propre dans la `FIFO_FREE_STATIONS`.
        
    - _Stratégie d'Expansion :_ Si la `FIFO_FREE_STATIONS` tend vers 0, il alloue de nouvelles tables dans la SHM.
        
    - _Stratégie de Contraction :_ Si la `FIFO_FREE_STATIONS` est saturée et le serveur calme, il intercepte les tables de la `FIFO_A_NETTOYER` et les libère du système Linux via `munmap`.
        

### ⚡ 2.2 Chronos (L'Aiguilleur Réseau)
- **`fifo_in_dispo` :** `FIFO_FREE_STATIONS` (Prend une table propre pour y injecter une requête brute)
    
- **`fifo_in_paquets` :** `ZYN_STATE_COMPRESSED` (Écoute les paquets terminés et compressés à envoyer sur le réseau)
    
- **`fifo_out_requetes` :** `FIFO_REQUESTED` (Pousse la table initialisée vers le premier cerveau mathématique)
    
- **`fifo_out_nettoyage` :** `FIFO_A_NETTOYER` (Jette la table usagée après expédition réseau du paquet)

Chronos est l'unique point d'entrée et de sortie I/O de la brique. Il doit rester ultra-rapide et ne fait aucun calcul mathématique.

- **Flux d'Entrée :** Il reçoit la requête réseau, pioche un poste dans la `FIFO_FREE_STATIONS`, y écrit le `Job_ID`, le `Macro_Chunk_ID` et les coordonnées ($X,Y,Z$) du Meso-Chunk (16m), puis jette le ticket dans la `FIFO_REQUESTED`.
    
- **Flux de Sortie :** Dès qu'un Token apparaît dans la file `ZYN_STATE_COMPRESSED`, Chronos extrait le paquet final pré-compressé depuis la SHM, l'envoie sur le socket réseau, nettoie son registre local, et jette instantanément le numéro de la table brute dans la `FIFO_A_NETTOYER`.
    

### 📐 2.3 Hécate (L'Analyseuse & Early Out)
- **`fifo_in` :** `FIFO_REQUESTED` (Prend les demandes de génération brute envoyées par Chronos)
    
- **`fifo_out_complexe` :** `FIFO_ANALYSED` (Propulse le jeton si le Meso-Chunk nécessite une forge volumétrique)
    
- **`fifo_out_early_out` :** `ZYN_STATE_COMPRESSED` (Court-circuite directement vers la sortie si le bloc est 100% uniforme)

Hécate est le premier cerveau mathématique. Son but est d'éviter le travail inutile en appliquant des filtres géométriques descendants.

- **Initialisation Offline (Boot) :** Calcule une matrice globale basse résolution des altitudes extrêmes du monde et la stocke dans un buffer local ultra-rapide.
    
- **Étape A (Early Out Flash) :** Interroge son buffer local. Si la hauteur $Y$ du Meso-Chunk est mathématiquement hors des reliefs possibles, elle applique un _Early Out_ immédiat. Elle marque le flag uniforme correspondant (100% Air, 100% Eau, ou 100% Roche), remplit l'en-tête et pousse le jeton directement à la fin (`ZYN_STATE_COMPRESSED`). **La DB et Atropos sont court-circuités.**
    
- **Étape B (La Forge Géologique) :** Si le bloc est dans la zone grise, elle lit les 5 points de contrôle réels et les biomes du Macro-Chunk (64m) en DB, et calcule les 4 points virtuels manquants.
    
- **Étape C (Spécification Meso-Chunk & Slow Early Out) :** 1. Elle résout l'interpolation biquadratique à l'échelle des 16m et écrit les 9 points, amplitudes et le biome principal locaux.
    
    2. Elle scanne les biomes des blocs voisins en DB et remplit le tableau `neighbor_biomes[4]`.
    
    3. _Slow Early Out :_ Grâce aux amplitudes, elle calcule l'altitude aux 4 coins des surfaces de transition (Air/Surface et Surface/Roche). Si le Meso-Chunk n'intersecte aucun de ces prismes de transition et n'a aucune frontière de biome active avec ses voisins, elle détermine sa matière unique, marque l'en-tête et l'envoie à la compression (`ZYN_STATE_COMPRESSED`).
    
    4. Si le bloc est intersecté par une surface ou une frontière active, il est déclaré `COMPLEXE` et envoyé dans la `FIFO_ANALYSED`.
    

### ⚒️ 2.4 Atropos (Le Chef de l'Atelier de Forge)
- **`fifo_in_global` :** `ZYN_STATE_ANALYSED` / `FIFO_ANALYSED` (Écoute les ordres complexes validés par Hécate)
    
- **`fifo_in_retour` :** `FIFO_RETOUR_FORGERONS` (Reçoit les comptes-rendus d'exécution des ouvriers forgerons)
    
- **`fifo_out_forgerons` :** `FIFO_MICRO_A_FORGER` (Distribue les micro-tickets de 4m aux ouvriers)
    
- **`fifo_out_global` :** `FIFO_FORGED` (Propulse les Meso-Chunks complets et validés vers l'étape de compression)

Atropos gère la répartition de la charge volumétrique. Il découpe le problème complexe en sous-tâches.

- **Cartographie et Distribution :** Il pioche dans la `FIFO_ANALYSED` et inspecte les 64 Micro-Chunks (4m) du châssis.
    
    - _Cas Uniformes :_ Si la sous-boîte de 4m est dans une couche homogène, il écrit l'état `UNIFORME` et l'ID de la matière.
        
    - _Cas Complexes :_ Si la boîte rencontre une surface ou une frontière, il la marque `A_FORGER`. Il calcule analytiquement les points d'entrée et de sortie de la frontière de biome à l'échelle de la boîte et écrit dans sa carte d'identité : `has_boundary = true`, le `neighbor_biome_id`, ainsi que les coordonnées locales `intersection_start` et `intersection_end`. Il génère une sous-carte géologique prémâchée pour le bloc.
        
- **Lancement et Suivi :** Il inscrit le nombre de blocs complexes dans `micro_chunks_pending`, enregistre le `Job_ID` dans son _Registre Local des Commandes en Cours_ (avec un timestamp), génère les micro-tickets dans la `FIFO_MICRO_A_FORGER` et passe immédiatement au Meso-Chunk suivant.
    
- **Clôture :** Écoute la `FIFO_RETOUR_FORGERONS`. Chaque retour décrémente `micro_chunks_pending`. Quand le compteur atteint **0**, le Meso-Chunk est complet. Atropos efface le job de son registre de surveillance, change l'état global et pousse le Token dans la `FIFO_FORGED`.
    
- **Patrouille de Sécurité :** Si un `Job_ID` traîne dans son registre local depuis plus de **200 ms**, Atropos déclare une défaillance ouvrière. Il inspecte la SHM, identifie les Micro-Chunks toujours bloqués à l'état `A_FORGER`, réinitialise leur buffer, recrée des micro-tickets tout neufs et les réinjecte dans la file des forgerons pour une reprise à zéro propre.
    

### 🛠️ 2.5 Les Forgerons (Les Ouvriers du Micro-Chunk)
- **`fifo_in` :** `FIFO_MICRO_A_FORGER` (File d'attente partagée où ils dorment en attente d'une tâche)
    
- **`fifo_out` :** `FIFO_RETOUR_FORGERONS` (File de compte-rendu où ils notifient Atropos du succès de leur forge)

Processus aveugles et intensifs, isolés dans leur cache CPU à l'échelle stricte des 4 mètres ($40 \times 40 \times 40 = 64\ 000$ voxels).

- **Initialisation :** Prépare un masque binaire local `uint64_t local_material_mask = 0` (64 matières natives max).
    
- **Triple Boucle de Forge ($X, Z, Y$) :** * _Frontière locale prémâchée :_ Si `has_boundary` est vrai, il calcule le bruit 1D déterministe contraint par `intersection_start` et `intersection_end` (garantissant le raccord parfait aux coins). Si la coordonnée franchit cette ligne, il applique les variables du `neighbor_biome_id`, sinon il reste sur le biome principal.
    
    - _Remplissage vertical :_ Compare la hauteur $Y$ absolue au sol biquadratique local ($Y_{\text{sol}}$). Au-dessus $\to$ Air (ou Eau sous le niveau de la mer). En dessous $\to$ Matière géologique selon la profondeur et les amplitudes.
        
    - _Recensement :_ À chaque voxel posé, il lève instantanément le bit associé dans son masque : `local_material_mask |= (1ULL << voxel_material_id)`.
        
- **Livraison :** Écrit le masque final dans la structure du Micro-Chunk, passe son état local à `FORGE`, jette un micro-ticket de confirmation dans la `FIFO_RETOUR_FORGERONS` pour notifier Atropos, et se rendort.
    

### 📐 2.6 Atlas (Le Compresseur / L'Emballeur)
- **`fifo_in` :** `FIFO_FORGED` (Écoute les Meso-Chunks complets validés par Atropos)
    
- **`fifo_out` :** `ZYN_STATE_COMPRESSED` (Propulse le paquet compacté vers Chronos pour expédition réseau)

Atlas prend le relais une fois le Meso-Chunk entièrement sculpté et validé dans la `FIFO_FORGED`. Il applique des algorithmes de sérialisation sans perte.

- **L'Analyse Flash :** N'effectue aucun scan de voxels en RAM. Il fait une opération **OU binaire (`|`)** sur les masques `uint64_t` des 64 Micro-Chunks. Via l'instruction processeur native `POPCNT` (`__builtin_popcountll`), il obtient en un cycle le nombre total de matériaux uniques présents dans tout le Meso-Chunk.
    
- **L'Arbitrage de Compression :**
    
    - _Mode Palette (Si $\le$ 4 matériaux différents) :_ Génère une table d'index locale et compresse le flux volumétrique en codant chaque voxel sur **2 bits seulement** au lieu de 8.
        
    - _Mode Direct + RLE (Si $>$ 4 matériaux) :_ Conserve l'encodage des matières sur 1 octet et applique une compression par répétition (RLE) sur le flux continu des Micro-Chunks complexes.
        
- **Emballage Final :** Écrit l'en-tête du paquet réseau (Manifeste des 64 blocs uniformes/complexes), y adjoint le flux compressé, mesure la taille totale, passe l'état global à `ZYN_STATE_COMPRESSED` et pousse le Token vers Chronos.


|**Nom de la FIFO**|**Producteur(s) (Qui écrit)**|**Consommateur(s) (Qui lit)**|**Rôle & Type de Donnée Transmise**|
|---|---|---|---|
|**`FIFO_FREE_STATIONS`**|🛡️ Cerbère|⚡ Chronos|**La Réserve :** Contient les indices (entiers) des tables de travail en SHM qui sont garanties propres, vides (remplies d'Air) et prêtes à l'emploi.|
|**`FIFO_REQUESTED`**|⚡ Chronos|📐 Hécate|**L'Entrée Usine :** Contient l'indice de la table qu'Hécate doit analyser. L'en-tête contient le `Job_ID` et les coordonnées géographiques du Meso-Chunk (16m).|
|**`FIFO_ANALYSED`**|📐 Hécate|⚒️ Atropos|**L'Atelier des Complexes :** Contient l'indice de la table pour les Meso-Chunks qui ont échoué au _Early Out_ d'Hécate et qui nécessitent une modélisation géologique.|
|**`FIFO_MICRO_A_FORGER`**|⚒️ Atropos|🛠️ Forgerons (Pool)|**La Distribution :** Contient un micro-ticket (Indice Table SHM + Index du Micro-Chunk de 0 à 63). Les forgerons s'y bousculent en parallèle pour piocher du travail.|
|**`FIFO_RETOUR_FORGERONS`**|🛠️ Forgerons (Pool)|⚒️ Atropos|**Le Compte-Rendu :** Contient le micro-ticket de confirmation renvoyé au Chef d'Atelier pour lui dire "Le Micro-Chunk $N$ est sculpté, son masque de bits est signé".|
|**`FIFO_FORGED`**|⚒️ Atropos|📐 Atlas|**Le Quai d'Emballage :** Contient l'indice de la table du Meso-Chunk dont les 64 Micro-Chunks sont tous au statut `UNIFORME` ou `FORGE`. Le bloc est prêt à être compacté.|
|**`ZYN_STATE_COMPRESSED`**|📐 Hécate & 📐 Atlas|⚡ Chronos|**La Sortie Usine :** Contient l'indice de la table où le paquet réseau final est packagé, mesuré et prêt à être envoyé sur le socket par Chronos.|
|**`FIFO_A_NETTOYER`**|⚡ Chronos|🛡️ Cerbère|**La Décontamination :** Contient l'indice de la table usagée qui vient d'être libérée après l'envoi réseau. Cerbère la récupère pour faire son `memset` à zéro.|
### 🔄 Visualisation du Flux Circulaire

Si on regarde le cycle de vie d'une seule table de travail (un châssis de la SHM), elle suit une boucle parfaite :

1. **`FIFO_FREE_STATIONS`** _(La table est propre)_
    
2. ➔ `Chronos` la prend, la remplit, et la jette dans **`FIFO_REQUESTED`**
    
3. ➔ `Hécate` la prend.
    
    - _Option A (Early Out) :_ Elle la propulse directement dans **`ZYN_STATE_COMPRESSED`** (Saute à l'étape 8).
        
    - _Option B (Complexe) :_ Elle la jette dans **`FIFO_ANALYSED`**.
        
4. ➔ `Atropos` la prend, découpe le travail et bombarde la **`FIFO_MICRO_A_FORGER`**.
    
5. ➔ Les `Forgerons` la squattent, écrivent les voxels, et répondent via **`FIFO_RETOUR_FORGERONS`**.
    
6. ➔ `Atropos` valide la fin des travaux et pousse la table dans **`FIFO_FORGED`**.
    
7. ➔ `Atlas` la prend, compresse la masse, et la pousse dans **`ZYN_STATE_COMPRESSED`**.
    
8. ➔ `Chronos` lit le paquet final, crache les octets sur le réseau, et jette la table vide dans **`FIFO_A_NETTOYER`**.
    
9. ➔ `Cerbère` la nettoie, remet les compteurs à zéro, et la réinjecte dans **`FIFO_FREE_STATIONS`**.