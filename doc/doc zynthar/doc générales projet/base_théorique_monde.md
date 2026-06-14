
# 🏗️ Synthèse Architecturale : Le Modèle Tripartite de Zynthar

Ce modèle théorique permet de résoudre les trois grands défis historiques des jeux de type voxel (Stockage, RAM et CPU) en séparant strictement les responsabilités.

## 1. Le Stockage (Base de données SQLite3 — Statique & Léger)

La base de données reste minuscule car elle ne contient aucun cube brut. Elle se divise en trois structures distinctes :

- **MacroChunks (Grille de 64m / ~134M de lignes) :** Stocke uniquement le squelette du monde, à savoir le type de biome, la température, l'humidité et l'élévation maximale théorique.
    
- **Rivers (Vectoriel / Table séparée) :** Stocke les lignes brisées des cours d'eau avec leur débit. Ces données servent simultanément pour le creusement déterministe du relief, la physique des bateaux et la génération d'énergie.
    
- **WorldDeltas (Dynamique) :** Enregistre exclusivement les actions des joueurs, comme les blocs posés ou cassés, ainsi que l'ancre des structures préfabriquées.
    

## 2. La Transition (Le CPU / Serveur — Mathématique & Déterministe)

Quand le joueur se déplace, le moteur détermine la grille de **Micro-Chunks** ($25,6\text{ m}$ de côté) à charger autour de lui dans un rayon maximal de 400 mètres.

### 📐 Calcul du "Pas" (LOD Radiaux)

Le CPU adapte la résolution de calcul du bruit mathématique selon la distance radiale:

- **$<25.6\text{ m}$ :** Échantillonnage toutes les 10 cm (Cubes de $10\text{ cm}$).
    
- **$25.6\text{ m}$ à $102.4\text{ m}$ :** Le CPU saute des étapes et calcule toutes les 40 cm (Cubes de $40\text{ cm}$).
    
- **$102.4\text{ m}$ à $409.6\text{ m}$ :** Le CPU calcule uniquement toutes les $1,6\text{ m}$ (Cubes de $1,6\text{ m}$).
    

> ⚙️ **Application des modifications :** Le CPU applique ensuite les deltas des joueurs correspondants à ces zones, en les adaptant à la résolution du niveau de LOD actuel.

## 3. L'Affichage (Le GPU / Client — WebGPU & Babylon.js)

L'affichage est conçu pour être ultra-rapide, optimisé pour les configurations légères type Chromebook.

- **Greedy Meshing :** Chaque Micro-Chunk génère son propre maillage (_Mesh_) unique via la fusion des faces de voxels identiques.
    
- **Gestion du cache :** Ces meshes sont stockés temporairement en RAM graphique (VRAM). Le cache de rendu est nettoyé en permanence au fil des déplacements du joueur.
    
- **Origine Flottante :** Le Micro-Chunk où se situe le joueur est cycliquement défini comme le centre $(0,0,0)$ de la scène Babylon.js. Cela permet d'annihiler le sautillement et le tremblement des vertices lointains causés par les limites de précision des variables `float32`.
    

## ⚡ Les Forces du Modèle : Élimination des "Trois Monstres"

| **Monstre Voxel**           | **Solution apportée par Zynthar**                                                                               | **Impact Réel**                                                                                 |
| --------------------------- | --------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------- |
| **Le problème du stockage** | Aucun cube brut n'est enregistré. Seul le squelette et les modifications (deltas) sont persistés.               | La base de données reste extrêmement légère et rapide à charger.                                |
| **Le problème de la RAM**   | Nettoyage continu du cache + maillages lointains d'un poids dérisoire grâce au LOD de $1,6\text{ m}$.           | Consommation de mémoire maîtrisée, même avec une distance d'affichage de 409.6 m.               |
| **Le problème du CPU**      | Restriction du calcul micro-précis ($10\text{ cm}$) à une bulle stricte de 25.6 mètres autour de l'utilisateur. | Le client (Chromebook) ne génère en haute résolution qu'une infime fraction du monde à la fois. |
