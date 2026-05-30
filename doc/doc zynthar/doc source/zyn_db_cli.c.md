# 🧰 Documentation Technique : `zyn_db_cli.c`

Ce module implémente l'outil d'interface en ligne de commande (CLI) écrit en C pur permettant d'initialiser, de maintenir, d'auditer et de stresser la base de données SQLite3 (`zyn-world.db`) qui héberge le squelette du monde de Zynthar.

## 📌 Rôle et Principes Fondamentaux

- **Squelette Macro-Procédural :** Ce programme administre la table des **MacroChunks** (~2 millions de lignes) contenant les métadonnées globales du terrain (biomes, climats, altitudes maximales).
    
- **Packing de Données Éco-Réseau :** L'outil convertit à la volée les unités physiques réelles (flottants en mètres, variables climatiques standardisées) en unités brutes ultra-légères (_integers_, _uint8_t_, _int16_t_) pour minimiser l'empreinte de la base et alléger la mémoire vive.
    

## 📐 Algorithmes de Conversion d'Unités (Macros de Packing)

Pour économiser de l'espace disque et accélérer les requêtes, les données stockées sont compressées via des opérations arithmétiques simples avant l'écriture, et décompressées à la lecture.

### 🌡️ Climatique (Flottant $\leftrightarrow$ Octet Brut)

Le climat est géré sous forme de pourcentage normalisé compris entre $0.0$ et $1.0$.

$$Raw = \lfloor Float \times 255.0 \rfloor$$

$$Float = \frac{Raw}{255.0}$$

### 🏔️ Altimétrique (Mètres $\leftrightarrow$ Décimètres)

L'altitude est stockée sous forme d'entier signé sur 16 bits (`int16_t`) représentant des décimètres ($dm$), éliminant ainsi le stockage lourd des nombres à virgule flottante (`float`).

$$Elevation_{dm} = \lfloor Elevation_{m} \times 10.0 \rfloor$$

$$Elevation_{m} = \frac{Elevation_{dm}}{10.0}$$

## 🛠️ Schéma de la Base de Données `macro_chunks`

La table gérée par ce CLI utilise la structure de stockage optimisée suivante :

SQL

```
CREATE TABLE IF NOT EXISTS macro_chunks (
    chunk_x INTEGER,
    chunk_z INTEGER,
    biome_type INTEGER NOT NULL,
    temperature INTEGER,
    humidity INTEGER,
    max_elevation INTEGER,
    PRIMARY KEY (chunk_x, chunk_z)
);
```

> ⚙️ **Optimisation de l'Index :** La clé primaire composite `PRIMARY KEY (chunk_x, chunk_z)` crée nativement un index B-Tree ultra-performant, permettant au serveur d'extraire instantanément les métadonnées d'une coordonnée précise lors du streaming du terrain.

## 🎛️ Commandes du CLI et Logiques Applicatives

### 1. `initw` (Initialisation)

- **Action :** Vérifie ou crée le dossier cible configuré via la variable d'environnement `ZYNTHAR_ROOT` (ou se rabat sur le dossier local `./`). Ouvre le fichier de base de données et exécute le script `CREATE TABLE IF NOT EXISTS` décrit ci-dessus.
    

### 2. `populate-random` (Remplissage de Masse)

- **Action :** Génère l'univers complet à des fins de test de charge. Elle calcule la taille maximale du monde en divisant les dimensions globales de Zynthar par la taille d'un Macro-Chunk ($512\text{ m}$).
    
- **Optimisations SQL critiques activées pendant l'injection :**
    
    - `PRAGMA synchronous = OFF;` : Pas d'attente du disque à chaque écriture.
        
    - `PRAGMA journal_mode = MEMORY;` : Journal de rollback conservé en RAM.
        
    - `PRAGMA cache_size = -80000;` : Allocation de 80 Mo de cache RAM dédié à l'écriture.
        
    - `PRAGMA locking_mode = EXCLUSIVE;` : Verrouillage total de la base par le processus pour accélérer l'écriture.
        

#### 📊 Flux d'Injection Massive

```
[Début populate-random]
         │
         ▼
[Vérification des bornes mondiales X / Z]
         │
         ▼
[Application des PRAGMAs de performance]
         │
         ▼
[Exécution de: BEGIN TRANSACTION;]
         │
         ▼
 ┌───► [Boucle sur la grille X] ────────────────────────┐
 │       │                                              │
 │       ▼                                              │
 │   ┌───► [Boucle sur la grille Z] ──────────────┐     │
 │   │       │                                    │     │
 │   │       ▼                                    │     │
 │   │   [Calcul PRNG Xorshift 32 bits]           │     │
 │   │   [Packing des données (Mètres -> dm)]     │     │
 │   │   [sqlite3_bind_* & sqlite3_step]          │     │
 │   │       │                                    │     │
 │   └───────┴────────────────────────────────────┘     │
 │       │                                              │
 └───────┴──────────────────────────────────────────────┘
         │
         ▼
[Exécution de: COMMIT;]
         │
         ▼
[Restauration du mode de verrouillage & Fin]
```

### 3. `stress-test` (Audit de Lecture Temps Réel)

- **Action :** Simule l'accès concurrent ou l'accès intensif du serveur en exécutant 10 000 requêtes `SELECT` aléatoires ciblées sur l'index composite.
    

#### 📊 Algorithme du Stress-Test

```
                [Début stress-test]
                        │
                        ▼
         [Préparation de la requête SELECT]
         [Activation du mode: read_uncommitted = TRUE]
                        │
                        ▼
   ┌───► [Boucle i allant de 0 à 9999] ──────────────────┐
   │       │                                             │
   │       ▼                                             │
   │   [Génération coordonnées aléatoires via Xorshift]  │
   │       │                                             │
   │       ▼                                             │
   │   [Liaison des index: sqlite3_bind_int(X et Z)]     │
   │       │                                             │
   │       ▼                                             │
   │   [sqlite3_step -> Mesure de l'existence du chunk]  │
   │   [sqlite3_reset]                                   │
   │       │                                             │
   └───────┴─────────────────────────────────────────────┘
                        │
                        ▼
         [Calcul de l'horloge système (clock_t)]
         [Affichage des métriques : Requêtes / Seconde]
                        │
                        ▼
                [Fin stress-test]
```

### 4. `export-csv` & `import-csv` (Interpérabilité)

- **`export-csv` :** Lit les données brutes de la base, applique les macros de conversion inverse (`RAW_TO_FLOAT` et `DM_TO_M`), puis écrit les résultats sous forme physique lisible par l'homme dans un fichier `.csv`.
    
- **`import-csv` :** Demande une confirmation de vidage de table à l'utilisateur, parse le fichier `.csv` ligne par ligne à l'aide de `sscanf`, pack les données textuelles en octets compacts, et effectue une insertion groupée sécurisée par une transaction atomique.
    

### 5. `info` (Statistiques)

- **Action :** Inspecte la table système de SQLite (`sqlite_master`) pour extraire le schéma de création exact de la base de données et exécute un décompte exact (`COUNT(*)`) des lignes enregistrées pour validation rapide.