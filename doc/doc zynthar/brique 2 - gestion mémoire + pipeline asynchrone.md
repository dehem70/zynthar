## 1. Philosophie & Modèle Mémoire

Pour éliminer la fragmentation de la RAM, supprimer les allocations dynamiques (`malloc`/`free`) en plein traitement et maximiser l'utilisation du cache CPU (_Data Locality_), la Brique 2 repose sur un modèle d'**Arène Adaptative de Pages Monolithiques**.

- **La Page Métier (16 Mo) :** Chaque Micro-Chunk en cours de fabrication est représenté par une page de mémoire brute contiguë de **16 777 216 octets** ($256 \times 256 \times 256$ voxels). Il n'y a plus de sous-tableaux de pointeurs ni de listes chaînées complexes pour les deltas.
    
- **L'Encapsulation (Pool Privé) :** Cette page de 16 Mo est intégrée dans une structure `MicroChunkContext` (contenant les métadonnées, coordonnées et altitudes des 4 coins), elle-même enveloppée dans un `PoolNode` invisible pour le reste du pipeline.
    
- **Le Régime de Croisière :** L'allocation se fait de manière adaptative en fonction de la charge par le Surveillant, mais une fois le pool stabilisé, la mémoire n'est plus que recyclée. **Zéro allocation en runtime.**
    

## 2. Cartographie des Rôles & Flux de Données

Le pipeline fonctionne de manière asynchrone en **temps masqué** à travers une séparation stricte des responsabilités :

### 🐕 1. Cerbère (Le Surveillant) – L'Intendant Mémoire

- **Rôle :** Gérer l'infrastructure en tâche de fond, totalement déconnecté du réseau et du calcul.
    
- **Mémoire :** Il maintient un seuil de sécurité de structures prêtes à l'emploi dans la `free_list` (Contextes + Pages de 16 Mo). C'est lui qui effectue les `malloc` si la charge monte, ou les `free` pour restituer la RAM à l'OS si les joueurs se déconnectent.
    

### ⏳ 2. Chronos (Worker 0) – Le Routeur Réseau & Écrivain

- **Rôle :** Réceptionner les paquets et interroger SQLite. Il ne fait aucun calcul lourd et n'alloue ni ne nettoie jamais de mémoire.
    
- **Mémoire :** Il pioche un contexte _chaud_ (déjà rendu 100% propre par Atlas) dans la `free_list`.
    
- **Action :** 1. Il extrait les deltas de SQLite et écrit l'ID matière **directement dans la page de 16 Mo** à l'index physique exact via la formule linéaire : `x + (y << 8) + (z << 16)`. Il y lève le bit de drapeau `0x80`.
    
    2. Il extrait les hauteurs des Macro-Chunks entourant la zone et calcule déterministement l'altitude absolue des **4 coins** de la région (via une simple moyenne).
    
    3. Il passe le pointeur de la structure métier à Atropos et retourne écouter le réseau.
    

### 📐 3. Atropos – Le Découpeur Spatial

- **Rôle :** Découper virtuellement le travail pour le paralléliser sans toucher aux voxels.
    
- **Action :** Il prend le contexte et génère une liste de **4 096 Nano-Jobs** (un par Nano-Chunk de $16^3$ voxels). Chaque Nano-Job contient simplement les coordonnées locales et pointe vers son secteur de 4 Ko dédié au sein de la page globale de 16 Mo. Il distribue ces jobs au pool de calcul.
    

### 🔨 4. Les Forgerons (Pool de Calcul) – Les Sculpteurs du Relief

- **Rôle :** Remplir les voxels manquants à la vitesse du silicium (Caches L1/L2).
    
- **Action :** Chaque Forgeron prend un Nano-Job (un bloc de 4 Ko dans les 16 Mo) :
    
    1. **Interpolation :** À partir des 4 altitudes de coins fournies dans le contexte, il calcule l'altitude de base lisse pour son Nano-Chunk.
        
    2. **Dual-Gate Early-Out (Sparsité Nano) :** * _Plein Ciel :_ Si le secteur est au-dessus du relief + marge ➔ Il n'y a que de l'air, il s'arrête immédiatement (les deltas/constructions sont déjà écrits).
        
        - _Plein Centre Terre :_ Si le secteur est sous le relief - marge ➔ C'est de la roche pure, il fait un `memset` de roche de 4 Ko sur sa zone (sans écraser les drapeaux deltas).
            
        - _Surface :_ Il calcule le bruit procédural lourd voxel par voxel, mais **court-circuite le calcul** si la case possède le drapeau `0x80` (présence d'un delta de joueur).
            

### 🗺️ 5. Atlas – Le Compresseur, Nettoyeur & Recycleur

- **Rôle :** Finaliser le travail, remettre la mémoire à neuf et restituer les ressources.
    
- **Action :** Il prend la page de 16 Mo maintenant entièrement complétée par les Forgerons. Grâce aux grands blocs uniformes générés par les _Early-Out_ (suites d'air ou de roche continue), son algorithme de compression **RLE** réduit le tout à une taille infime pour la Brique 3 et l'envoi réseau.
    
- **Nettoyage Mémoire :** Profitant du fait que la page de 16 Mo est encore "chaude" dans les lignes de cache de son processeur, Atlas exécute immédiatement un `memset(context->voxels_page, 0, 16777216)` après la compression. Il efface ainsi toutes les matières et les drapeaux `0x80`.
    
- **Recyclage :** Une fois la page remise à zéro, il réinjecte le `PoolNode` dans la `free_list`. Le contexte est ainsi garanti 100% vierge pour la prochaine pioche de Chronos.