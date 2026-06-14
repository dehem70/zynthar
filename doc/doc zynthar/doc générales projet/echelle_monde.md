# 📐 Échelles, Métriques et LOD du Monde

Ce document de référence technique synthétise les dimensions absolues, les structures de découpage, la gestion du niveau de détail (LOD) et les règles arithmétiques physiques de **Zynthar v0.1**.

## 1. L'Échelle du Monde (Macro)

Les dimensions globales du monde sont définies pour offrir une envergure réaliste tout en maintenant un repère hydrographique fixe.

| **Paramètre**              | **Valeur**                    | **Équivalence Réelle / Voxels**                                 |
| -------------------------- | ----------------------------- | --------------------------------------------------------------- |
| **Longueur du monde**      | $1\,048\text{ km}$            | $1\,048\,576\text{ m}$                                          |
| **Largeur du monde**       | $524\text{ km}$               | $524\,288\text{ m}$                                             |
| **Hauteur totale**         | $3\,072\text{ m}$             | De $-1\,024\text{ m}$ (profondeurs) à $+2\,048\text{ m}$ (ciel) |
| **Niveau de la mer**       | $0\text{ m}$                  | Point d'ancrage hydrographique                                  |
| **Volume total potentiel** | $\approx 1,68 \times 10^{18}$ | $1,68$ milliards de milliards de micro-voxels                   |

## 2. Le Découpage Structurel (Base de données & Rendu)

La gestion de l'univers repose sur une architecture quadripartite imbriquée (Voxel $\rightarrow$ Micro $\rightarrow$ Meso $\rightarrow$Macro) afin d'optimiser l'occupation mémoire.

| **Structure**   | **Dimensions Réelles**                                                                  | **Dimensions en Voxels**        | **Rôle / Stockage**                                                                                            |
| --------------- | --------------------------------------------------------------------------------------- | ------------------------------- | -------------------------------------------------------------------------------------------------------------- |
| **Micro-Voxel** | $10 \times 10 \times 10\text{ cm}$                                                      | $1 \times 1 \times 1$           | L'unité cubique de base du monde.                                                                              |
| Région          | $4\,096 \times 4\,096\text{ m}$<br><br>  <br><br>_(sur $\approx $3\text{ km}$ de haut)_ |                                 | structure virtuelle pour l'index en base de données<br><br>  <br>                                              |
| **Macro-Chunk** | $64 \times 64\text{ m}$<br><br>  <br><br>_(sur $\approx $3\text{ km}$ de haut)_         | $640 \times 640 \times 30\,720$ | **En base SQLite3** (~134M de lignes) .<br><br>  <br><br>Gère le biome, le climat, les rivières et les deltas. |
| **Meso-Chunk**  | $16 \times 16 \times 16\text{ m}$                                                       | $160 \times 160 \times 160$     | **En RAM uniquement** .<br><br>  <br><br>                                                                      |
| **Micro-Chunk** | $4 \times 4 \times 4\text{ m}$                                                          | $40 \times 40 \times 40$        | **En RAM uniquement** .<br><br>  <br><br>L'unité de maillage (_Greedy Meshing_) et de collision physique.      |

## 3. Le Modèle Hybride de Rendu (LOD Radiaux)

Le niveau de détail (_Level of Detail_ - LOD) des Micro-Chunks s'adapte dynamiquement selon la distance par rapport au joueur pour préserver les performances du client (notamment les configurations Chromebook PWA).

| **Palier de Distance**    | **Niveau de LOD** | **Taille des Cubes (Précision)** | **Résolution du Micro-Chunk**        | **Impact Visuel / Physique**                                          |
| ------------------------- | ----------------- | -------------------------------- | ------------------------------------ | --------------------------------------------------------------------- |
| **0 à 16 mètres**         | **LOD 0**         | $10\text{ cm}$                   | $40 \times 40 \times 40$ voxels      | Précision chirurgicale, collisions physiques actives.                 |
| **16 à 64 mètres**        | **LOD 1**         | $40\text{ cm}$                   | $10 \times 10 \times 10$ voxels_LOD1 | Transition visuelle fluide en moyenne portée.                         |
| **64 à 256 mètres**       | **LOD 2**         | $2\text{ m}$                     | $2 \times 2 \times 2$ voxels_LOD2    | Décor lointain ultra-léger en longue portée.                          |
| **256 à 512 mètres**      | **LOD 3**         | $4\text{ m}$                     | $1 \times 1 \times 1$ voxels_LOD3    | Décor très lointain méga-léger pour l'horizon                         |
| **Au-delà de 512 mètres** | —                 | —                                | —                                    | Brouillard / Non affiché. Limite de la grille de streaming graphique. |

## 4. Les Physiques de Déplacement (Gameplay)

Lorsqu'un joueur avance face à un dénivelé ou un obstacle vertical, le moteur évalue sa hauteur en fonction de quatre plages arithmétiques strictes:

| **Hauteur à franchir**                | **Équivalence en Voxels** | **Type d'Action**                | **Commentaire**                                                                |
| ------------------------------------- | ------------------------- | -------------------------------- | ------------------------------------------------------------------------------ |
| **$\le 40\text{ cm}$**                | 1 à 4 blocs               | **Marche automatique**           | Le joueur franchit l'obstacle sans action requise (ex: trottoir, racine).      |
| **$41\text{ cm}$ à $120\text{ cm}$**  | 5 à 12 blocs              | **Enjambement / Escalade basse** | Obstacle franchissable via une interaction simple (bouton Sauter).             |
| **$121\text{ cm}$ à $200\text{ cm}$** | 13 à 20 blocs             | **Escalade haute**               | Le personnage doit activement se hisser à la force des bras.                   |
| **$> 200\text{ cm}$**                 | Plus de 20 blocs          | **Mur infranchissable**          | L'obstacle dépasse la ligne des yeux/buste du personnage, bloquant le passage. |
## 3. Polymorphisme et Volumétrie du Micro-Chunk (4m)

Le Micro-Chunk change de structure selon l'endroit où il se trouve dans le pipeline pour maximiser les performances de la RAM et du CPU.

### A. Le Format de Transit Externe (Entrées / Sorties de la Brique 2)

Pour ne pas saturer la bande passante réseau et la mémoire globale, les Micro-Chunks en dehors de la zone de calcul sont compressés selon trois états :

1. **L'état Uniforme (Homogène) :** Si le bloc est 100% vide (ciel) ou 100% plein (roche profonde).
    
    - _Taille en mémoire :_ **2 octets** (1 octet header + 1 octet pour L'ID de la matière).
        
2. **L'état Palétisé (Semi-complexe) :** Si le bloc contient entre 2 et 15 matières différentes (Surface du monde, grottes). Les voxels sont compactés par demi-octets (4 bits par voxel).
    
    - _Taille en mémoire :_ Header (1 octets) + Palette (15 octets) + Data (32 000 octets) = **32,01 Ko**.
        
3. **L'état RLE / Réseau :** Compression par plage de répétition pour le flux réseau vers le client B3.
    
    - _Taille moyenne d'un paquet :_ **~1 Ko** .
        

### B. Le Format de Travail Interne (Cœur de la Brique 2)

Dès qu'un travailleur de la Brique 2 doit générer, modifier ou appliquer une action de minage sur un Micro-Chunk, **le bloc est instantanément déballé en mode RAW pur**.

- **Structure :** Tableau C linéaire (`uint8_t[64000]`), à raison de 1 octet par voxel (permettant de gérer jusqu'à 256 matières globales).
    
- **Taille en RAM :** Strictement **64 Ko**.