# 📊 Diagramme de Flux et Architecture Mémoire — Brique 2 (V2.1)

```mermaid
graph TD
    %% Déclaration des Nœuds
    B3["Brique 3 : Réseau"]
    W0["🧩 Worker 0 : I/O & SQLite3"]
    W1["✂️ Worker 1 : Ordonnanceur & Filtre AABB"]
    WC1["🧮 Worker C1"]
    WC2["🧮 Worker C2"]
    WCx["🧮 Worker Cx"]
    W2["🗜️ Worker 2 : Compresseur RLE Élite"]
    SV["🛡️ Le Surveillant Indépendant"]

    %% Définition des sous-graphes (Sécurisés avec guillemets)
    subgraph B2 ["Brique 2 : Persistance & Génération"]
        W0
        W1
        subgraph Pool ["Pool de Workers de Calcul"]
            WC1
            WC2
            WCx
        end
        W2
    end

    %% Liens et flux de données
    B3 <--> W0
    W0 --> W1
    W1 --> WC1
    W1 --> WC2
    W1 --> WCx
    
    WC1 --> W2
    WC2 --> W2
    WCx --> W2
    
    W2 --> W0

    %% Liens de Supervision
    SV -.-> W0
    SV -.-> Pool

    %% Styles CSS pour Obsidian
    classDef network fill:#2b6cb0,stroke:#3182ce,stroke-width:2px,color:#fff;
    classDef worker fill:#2d3748,stroke:#4a5568,stroke-width:2px,color:#fff;
    classDef supervisor fill:#742a2a,stroke:#9b2c2c,stroke-width:2px,color:#fff;

    %% Application des styles
    class B3 network;
    class W0,W1,WC1,WC2,WCx,W2 worker;
    class SV supervisor;
```
    
## 📌 Légende Technique des Flux de Données

1. **Ligne descendante (Entrée de Tâches) :** Requêtes réseau légères FlatBuffers $\rightarrow$ Métadonnées SQLite lues par paquets $\rightarrow$ Découpe en structures atomiques `NanoJob` de 64 octets alignées pour le cache.
    
2. **Zone de Turbulence Calcul (Pool) :** Isolation complète. Les données voxel brutes ne naissent et ne transitent que dans les lignes de **Cache L1/L2 des cœurs CPU** ($> 1\text{ To/s}$). La RAM principale n'est jamais polluée par des matrices brutes non compressées.
    
3. **Ligne ascendante (Sortie Réseau) :** Le compresseur écrase le superflu $\rightarrow$ Seul le fluide binaire RLE compact est persisté dans le cache RAM global et streamé sur le réseau par la Brique 3.
    
4. **Le Cadre Externe (Surveillant) :** N'interfère jamais avec la mémoire des workers pendant qu'ils calculent. Il agit de manière asynchrone sur l'OS (`ramdisk`, fichiers, signaux de threads, auto-scaling de la pool).