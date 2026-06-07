[ Brique 3 : Réseau / AoI ]
       │
       │ (Appel fonction avec ID, Local_XYZ, LOD)
       ▼
┌── ──────────────────────────────────────────────┐
│  BRIQUE 2 : MOTEUR DE PERSISTANCE & GÉNÉRATION LOCALE                          │
│                                                                                                                                                      │
│  1. ENTRÉE API :                                                                                                                      │
│     B2_GenerateMicroChunk(macro_id, mc_x, mc_y, mc_z, lod_level)                   │
│                                                                                                                                                      │
│  2. COUCHE D'AGRÉGATION (Cache RAM / SQLite3)                                                  │
│     Vérification du Cache RAM Macro-Chunk                                                                  │
│        ├──► [ HIT ]  Récupération directe des structures 8-octets                           │
│        └──► [ MISS ] Requête SQL (ATTACH DATABASE)                                             │
│                         │                                                                                                                          │
│                         ├──► world_core.db    ──► MacroChunk   (8 octets)                     │
│                         └──► world_rivers.db  ──► ZynRiverNode (8 octets)                    │
│                                                                                                                                                      │
│  3. PIPELINE DE SIMULATION & DÉCORATION                                                             │
│     │                                                                                                                                              │
│     ├──► Étape A : Bruit Mathématique Déterministe                                              │
│     │    (Utilise : Seed Globale, Coordonnées, elevation_max_dm)                           │
│     │    └──► Génère la matrice de relief brute adaptée au LOD (256³,64³,16³)│
│     │                                                                                                                                              │
│     └──► Étape B : Décorateur de Biomes                                                                     │
│          (Utilise : biome, temperature_raw, data [River Flow/Dir])                               │
│          └──► Remplace les blocs (Air, Eau, Sable, Roche, Terre)                               │
│                                                                                                                                                      │
│  4. ASSEMBLEUR DE CHUNK (Fusion des Anomalies)                                                 │
│     │                                                                                                                                              │
│     ├──► Requête SQLite3 (world_deltas)                                                                    │
│     │    └──► Extrait les blocs modifiés par les joueurs pour ce Micro-Chunk    │
│     │                                                                                                                                              │
│     └──► Fusion : Écrase les voxels algorithmiques par les Deltas                         │
│                                                                                                                                                      │
│  5. SORTIE API :                                                                                                                       │
│     Renvoie la structure ActiveMicroChunk prête pour le RLE                                  │
└────────────────────────────────────────────────┘
       │
       │ (Pointeur *voxels brut + lod_level)
       ▼
[ Brique 3 : Compresseur RLE ] ──► [ Pipeline Réseau ]