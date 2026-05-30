# 🚶 Physique d'Exploration & Level Design Déterministe

## 📊 Configuration des Seuils de Franchissement

| Hauteur de l'obstacle | Équivalence en Voxels       | Type d'Action                            | Comportement en jeu / Description                                                                  |
| --------------------- | --------------------------- | ---------------------------------------- | -------------------------------------------------------------------------------------------------- |
| **≤40 cm**            | Jusqu'à 4 blocs<br><br>     | **Marche automatique**<br><br>           | Le joueur franchit l'obstacle sans s'en rendre compte (ex: trottoir, petite racine).<br><br>       |
| **40 cm à 1 m**       | De 5 à 10 blocs<br><br>     | **Enjambement / Escalade basse**<br><br> | Obstacle franchissable (ex: muret, gros rocher) à l'aide du bouton **"Sauter"**.<br><br>           |
| **1 m à 1,6 m**       | De 11 à 16 blocs<br><br>    | **Escalade haute**<br><br>               | Le personnage doit se hisser activement à la force des bras via le bouton **"Escalader"**.<br><br> |
| **>1,6 m**            | Au-delà de 16 blocs<br><br> | **Mur infranchissable**<br><br>          | Obstacle totalement bloquant qui dépasse la ligne de vision ou du buste.                           |

## 🎯 L'impact de ces seuils sur le Level Design Déterministe

Ce choix de valeurs va profondément influencer la façon dont le monde de Zynthar va être généré mathématiquement. Puisque vous connaissez précisément les capacités physiques du joueur, votre algorithme micro-déterministe peut directement s'en servir pour sculpter le relief.

### 1. La gestion des pentes et des sentiers

- **Contrôle du relief :** Si le bruit de Perlin génère une colline, l'algorithme qui calcule le maillage peut s'assurer de ne pas créer des successions de "marches" de plus de 40 cm sur les axes de passage principaux, comme les bords des rivières vectorielles.
       
- **Rythme d'exploration :** Si une pente naturelle fait par défaut des sauts de 60 cm, l'algorithme sait instantanément que cette colline sera une zone d'escalade, modifiant ainsi l'expérience et le rythme de l'exploration pour le joueur.
    
### 2. L'intégration avec le système de LOD (Le piège des 1,6 m lointains)

Rappelez-vous notre modèle hybride : à 300 mètres du joueur, le monde est affiché avec des gros blocs de 1,6 m de côté (**LOD 2**).



> ⚠️ **Le problème physique :** > À 300 mètres, une falaise de LOD 2 affichera une seule marche géante de 1,6 m. Selon vos seuils, 1,6 m est pile la limite de l'infranchissable. Une telle marche est bloquante. Cependant, en s'approchant (**LOD 0** à précision 10 cm), cette même falaise est peut-être en réalité un escalier naturel de quatre marches de 40 cm, donc parfaitement franchissable en marchant.

- **La solution théorique :** Les calculs physiques de collision et de déplacement du joueur ne doivent **jamais** se faire sur le maillage visuel lointain (LOD 1 ou LOD 2).
       
- **Autorité Serveur :** Le serveur calcule la physique uniquement dans la **bulle haute résolution** (LOD 0, cubes de 10 cm) entourant le joueur.
       
- **Rendu vs Réalité :** Ce que le joueur voit au loin n'est qu'un décor ; la réalité physique sous ses pieds reste toujours précise à 10 cm près.
    

## 📌 Règle de Cohérence de Gameplay

La gestion de la **sphère d'interaction** et de la **distance minimale** entre les entités doit être intégrée impérativement, car elle limite les conflits de voxels et assainit le gameplay global.

