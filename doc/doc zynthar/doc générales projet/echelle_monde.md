Voici la mise en forme Markdown complète et structurée de votre document sur les échelles et métriques de l'univers de Zynthar. J'ai converti les blocs de texte brut en véritables tableaux Markdown pour garantir une lisibilité optimale et une scannabilité immédiate.

# 📐 Échelles, Métriques et LOD du Monde

Ce document de référence technique synthétise les dimensions absolues, les structures de découpage, la gestion du niveau de détail (LOD) et les règles arithmétiques physiques de **Zynthar v0.1**.

## 1. L'Échelle du Monde (Macro)

Les dimensions globales du monde sont définies pour offrir une envergure réaliste tout en maintenant un repère hydrographique fixe.

| **Paramètre**              | **Valeur**                   | **Équivalence Réelle / Voxels**                                 |
| -------------------------- | ---------------------------- | --------------------------------------------------------------- |
| **Longueur du monde**      | $1\,000.96\text{ km}$        | $1\,000\,960\text{ m}$                                          |
| **Largeur du monde**       | $500.224\text{ km}$          | $500\,224\text{ m}$                                             |
| **Hauteur totale**         | $3\,072\text{ m}$            | De $-1\,024\text{ m}$ (profondeurs) à $+2\,048\text{ m}$ (ciel) |
| **Niveau de la mer**       | $0\text{ m}$                 | Point d'ancrage hydrographique                                  |
| **Volume total potentiel** | $\approx 1,5 \times 10^{15}$ | $1,5$ million de milliards de micro-voxels                      |

## 2. Le Découpage Structurel (Base de données & Rendu)

La gestion de l'univers repose sur une architecture tripartite imbriquée (Voxel $\rightarrow$ Micro $\rightarrow$ Macro) afin d'optimiser l'occupation mémoire.

| **Structure**   | **Dimensions Réelles**                                                            | **Dimensions en Voxels**              | **Rôle / Stockage**                                                                                          |
| --------------- | --------------------------------------------------------------------------------- | ------------------------------------- | ------------------------------------------------------------------------------------------------------------ |
| **Micro-Voxel** | $10 \times 10 \times 10\text{ cm}$                                                | $1 \times 1 \times 1$                 | L'unité cubique de base du monde.                                                                            |
| **Macro-Chunk** | $512 \times 512\text{ m}$<br><br>  <br><br>_(sur $\approx $3\text{ km}$ de haut)_ | $5\,120 \times 5\,120 \times 30\,720$ | **En base SQLite3** (~2M de lignes) .<br><br>  <br><br>Gère le biome, le climat, les rivières et les deltas. |
| **Micro-Chunk** | $25,6 \times 25,6 \times 25,6\text{ m}$                                           | $256 \times 256 \times 256$           | **En RAM uniquement** .<br><br>  <br><br>L'unité de maillage (_Greedy Meshing_) et de collision physique.    |

## 3. Le Modèle Hybride de Rendu (LOD Radiaux)

Le niveau de détail (_Level of Detail_ - LOD) des Micro-Chunks s'adapte dynamiquement selon la distance par rapport au joueur pour préserver les performances du client (notamment les configurations Chromebook PWA).

| **Palier de Distance**      | **Niveau de LOD** | **Taille des Cubes (Précision)** | **Résolution du Micro-Chunk**      | **Impact Visuel / Physique**                                          |
| --------------------------- | ----------------- | -------------------------------- | ---------------------------------- | --------------------------------------------------------------------- |
| **0 à 25.6 mètres**         | **LOD 0**         | $10\text{ cm}$                   | $256 \times 256 \times 256$ voxels | Précision chirurgicale, collisions physiques actives.                 |
| **25.6 à 102.4 mètres**     | **LOD 1**         | $40\text{ cm}$                   | $64 \times 64 \times 64$ voxels    | Transition visuelle fluide en moyenne portée.                         |
| **102.4 à 409.6 mètres**    | **LOD 2**         | $1,6\text{ m}$                   | $16 \times 16 \times 16$ voxels    | Décor lointain ultra-léger pour l'horizon.                            |
| **Au-delà de 409.6 mètres** | —                 | —                                | —                                  | Brouillard / Non affiché. Limite de la grille de streaming graphique. |

## 4. Les Physiques de Déplacement (Gameplay)

Lorsqu'un joueur avance face à un dénivelé ou un obstacle vertical, le moteur évalue sa hauteur en fonction de quatre plages arithmétiques strictes:

|**Hauteur à franchir**|**Équivalence en Voxels**|**Type d'Action**|**Commentaire**|
|---|---|---|---|
|**$\le 40\text{ cm}$**|1 à 4 blocs|**Marche automatique**|Le joueur franchit l'obstacle sans action requise (ex: trottoir, racine).|
|**$41\text{ cm}$ à $100\text{ cm}$**|5 à 10 blocs|**Enjambement / Escalade basse**|Obstacle franchissable via une interaction simple (bouton Sauter).|
|**$101\text{ cm}$ à $160\text{ cm}$**|11 à 16 blocs|**Escalade haute**|Le personnage doit activement se hisser à la force des bras.|
|**$> 160\text{ cm}$**|Plus de 16 blocs|**Mur infranchissable**|L'obstacle dépasse la ligne des yeux/buste du personnage, bloquant le passage.|

## 📌 Note pour l'implémentation du code futur

> Toutes les dimensions de nos Micro-Chunks et de nos niveaux de LOD sont des **puissances de 2** ($256, 64, 16$). Cela permettra au serveur en C et au client en TypeScript d'utiliser des **opérations binaires par décalage de bits** (`<<` et `>>`) directement au niveau du CPU, optimisant drastiquement les performances de calcul en temps réel.