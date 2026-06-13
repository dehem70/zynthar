# 📑 SPÉCIFICATION TECHNIQUE RATIFIÉE : WORKER HÉCATE (V0.1.0)

> **Rôle :** Gardienne et gestionnaire exclusive de l'Arbre d'Uniformité en RAM (Index de Shunt Éclair pour Chronos).
> 
> **Autorité :** Seul thread du système autorisé à _écrire_ (modifier) la structure de l'arbre en RAM. Chronos possède un accès concurrent en _lecture seule_ ultra-rapide ($O(1)$ à $O(3)$).

### 🧱 1. ARCHITECTURE DE L'ARBRE EN RAM (3 NIVEAUX)

Pour s'adapter à la topologie géologique du monde (ciel vide, roche profonde pleine, surface complexe) tout en minimisant l'empreinte RAM, Hécate maintient une structure arborescente conditionnelle :

```
[ Niveau 1 : MACRO-CHUNK (Racine) ]
                |
       (Si complexe / mix)
                v
[ Niveau 2 : TRANCHE DE HAUTEUR (Couche) ] -> Tableau fixe de 16 tranches
                |
       (Si complexe / mix)
                v
[ Niveau 3 : MICRO-CHUNK (Feuilles) ] ------> 4096 feuilles (Micro-Chunks de 4 Ko)
```

#### États possibles d'un nœud (représentés sur 1 octet) :

- `0x00` : **AIR** (Uniforme) $\rightarrow$ Fin de branche.
    
- `0x01` : **ROCHE** (Uniforme) $\rightarrow$ Fin de branche.
    
- `0xFF` : **COMPLEXE** (Hétérogène) $\rightarrow$ Pointe vers le niveau inférieur.
    

### 🟢 2. PHASE AUTOMNE : INITIALISATION "OFFLINE" (BOOT)

Cette phase s'exécute au démarrage de la Brique 2, avant l'ouverture du réseau par Cerbère.

1. **Scan des métadonnées :** Hécate ouvre les bases SQLite `.db` sur le Ramdisk. Elle lit uniquement la table légère des métadonnées des Macro-Chunks (elle ignore et ne charge pas les gros BLOBs de voxels).
    
2. **Construction de l'Arbre :**
    
    - Si le Macro-Chunk est marqué uniforme en DB, elle alloue **1 octet** au Niveau 1 (`0x00` ou `0x01`).
        
    - Si le Macro-Chunk est marqué complexe, elle alloue le Niveau 2 (16 tranches). Pour chaque tranche uniforme, elle pose un octet de statut. Pour chaque tranche complexe, elle alloue les 4096 feuilles du Niveau 3.
        
3. **Signal "Feu Vert" :** Une fois la RAM intégralement indexée, Hécate lève un flag atomique ou émet un signal à destination de Cerbère. Le réseau (Chronos) et le calcul (Atropos) peuvent démarrer.
    

### 🔵 3. PHASE DYNAMIQUE : GESTION "ONLINE" (BOUCLE DE VIE)

Une fois le serveur actif, Hécate tourne en tâche de fond (_Background Worker Thread_) sans jamais bloquer l'I/O réseau de Chronos.

#### A. Interface d'entrée (I-O)

Hécate consomme en continu une file asynchrone non-bloquante (`hecate_update_queue`) alimentée par Chronos lors des modifications du monde.

C

```
typedef struct {
    uint64_t macro_id;     // ID du Macro-Bloc cible
    uint16_t micro_id;     // ID du Micro-Chunk (0 à 4095)
    uint8_t  action_type;  // 0 = Voxel posé, 1 = Voxel détruit
    uint8_t  voxel_id;     // Type du voxel modifié
} HecateTicket;
```

#### B. Logique d'alignement de la RAM (Algorithme de Mutation)

À la réception d'un ticket, Hécate localise la position dans l'arbre :

1. **Si le Micro-Chunk cible (Niveau 3) est déjà marqué `COMPLEXE` (`0xFF`) :**
    
    - _Action :_ Hécate jette le ticket. La structure est déjà prête pour Atropos.
        
2. **Si la Tranche (Niveau 2) ou le Micro-Chunk (Niveau 3) était `UNIFORME` :**
    
    - _Action :_ Hécate mute le flag à `COMPLEXE` (`0xFF`).
        
    - _Allocation dynamique :_ Si la tranche supérieure était uniforme, elle alloue le sous-tableau de feuilles (Niveau 3) à la volée, initialise les voisins avec l'ancienne valeur uniforme, et marque le Micro-Chunk touché comme `COMPLEXE`.
        
    - _Résultat :_ Dès la microseconde suivante, la lecture de Chronos bifurquera instantanément vers le pipeline lourd SQLite/Atropos pour cette zone précise.
        

### 🎯 4. LE CONTRAT CHRONOS (RAPPEL)

Grâce au travail d'Hécate, la logique de lecture de Chronos est gravée ainsi :

- Chronos interroge l'Arbre en RAM d'Hécate.
    
- **Si `0x00` ou `0x01` :** Shunt Éclair. Chronos répond immédiatement 2 octets au client web (LZ4 évité, SQLite évité, Atropos évité).
    
- **Si `0xFF` :** Chronos extrait le BLOB de SQLite et délègue le job à Atropos.