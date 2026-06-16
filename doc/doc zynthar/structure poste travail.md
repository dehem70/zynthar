🗺️ Structure Globale d'un Poste de Travail (Fichier SHM)
Le fichier est découpé en 5 grandes sections consécutives en mémoire :

SECTION 1 : L'En-tête de Contrôle et Routage (Le "Dashboard")
Cette zone permet de piloter le poste de travail et de savoir où il se situe dans le monde et dans le réseau.

ID du Job / Requête : Identifiant réseau de la demande client (pour renvoyer le bloc au bon joueur).

ID du Macro-Chunk : L'identifiant de la grande région parente de 256 m.

Coordonnées Meso relatives (X, Y, Z) : La position exacte de ce Meso-Chunk (16 m) à l'intérieur de son Macro-Chunk.

Statut Global du Poste : L'état d'avancement de la table (ex: LIBRE, EN_ANALYSE, EN_COURS_DE_FORGE, PRET_A_COMPRESSER, COMPRESSE).

Compteur Atomique de Micro-Chunks restants : Initialisé par Atropos après son analyse. Chaque forgeron qui termine un bloc décrémente ce compteur. Quand il atteint 0, le Meso-Chunk est entièrement forgé.

📐 SECTION 2 : L'Empreinte Géologique Majeure (Niveau Meso-Chunk - 16 m)
Cette section contient la structure macroscopique complète calculée en amont :

La Topographie et Relief de Base (Le squelette géométrique)

Altitudes de Référence : Les hauteurs théoriques calculées aux 4 coins et au centre du Meso-Chunk (5 valeurs flottantes). C'est le plan de base incliné ou incurvé de cette portion du monde.

Les 4 Amplitudes sur les 4 Arêtes Verticales : Calculées aux quatre coins, elles modulent ce plan de base pour définir les variations de relief locales et dictent également l'épaisseur de la couche de surface (la transition terre/roche).

La Signature des Biomes (La texture et le style de terrain)

ID du Biome Majeur : L'identifiant du biome dominant au centre du Meso-Chunk (qui fixe implicitement les règles de sédimentation et de végétation).

Tableau des Biomes Adjacents et Facteurs de Mixage : Les IDs des biomes voisins et leurs poids d'influence pour réaliser le fondu élégant et progressif des formes et des types de blocs.

### 📐 Structuration de la SECTION 3 (Niveau Micro-Chunk - $4\text{ m}$)

Grâce à cette règle déterministe, on peut jeter la liste de matériaux complexe et la remplacer par une structure topologique pure, sans aucun `if` pour le Forgeron.

Voici comment on organise une case du tableau de la Section 3 pour un Micro-Chunk :

1. **Le Statut Local (1 octet) :**
    
    - `HOMOGENE` ou `A_FORGER`.
        
2. **Le Masque de Matériaux (8 octets / `uint64_t`) :**
    
    - Toujours présent pour la cohérence globale et l'indexation rapide (et pour le cas `HOMOGENE` comme validé précédemment).
        
3. **Le Couple de Matériaux de Frontière (2 octets) :**
    
    - **ID Matériau Supérieur (`uint8_t`) :** Le matériau situé au-dessus de la limite (ex: Terre/Herbe).
        
    - **ID Matériau Inférieur (`uint8_t`) :** Le matériau situé en dessous de la limite (ex: Roche).
        
4. **Les Données de Limite Déterministe 1D (Quelques octets) :**
    
    - Les paramètres mathématiques générés par Atropos pour interpoler la frontière exacte entre la couche supérieure et inférieure à l'intérieur de ce bloc (hauteur de départ de la frontière, inclinaison ou coordonnées packagées du plan de coupe).
#### SECTION 4 : Le Buffer Volumétrique Brut (La Zone de Forge)

C'est la zone mémoire la plus lourde, là où les forgerons écrivent les voxels sculptés.

- Un tableau à deux dimensions : `[64][64000] octets` (puisqu'un Micro-Chunk à résolution maximale contient $40 \times 40 \times 40 = 64\ 000$ voxels).
    
- **Fonctionnement :** Seules les cases du tableau correspondant à des Micro-Chunks marqués `A_FORGER` seront remplies par les forgerons. Les autres restent à zéro (Air).

#### SECTION 5 : La Zone d'Empaquetage Réseau (Le Produit Fini)

Une fois que le compteur atomique tombe à 0, le module de compression (Atropos ou un compresseur dédié) lit le Buffer Volumétrique Brut, compresse les données, et écrit le résultat ici.

- **Buffer Réseau :** Une zone de mémoire brute de taille fixe contenant le Meso-Chunk compressé, prêt à être injecté dans la carte réseau.
    
- **Taille Réelle du Paquet :** Un entier indiquant le nombre exact d'octets occupés par le flux compressé dans le buffer.