import sqlite3

# =============================================================================
# CACHES / BUFFERS EN MÉMOIRE RAM
# =============================================================================
# Ces structures stockent les correspondances { "nom_du_biome": id_numerique }
# pour éviter de faire un SELECT à chaque tuile.
CACHES = {
    "type": {},
    "biome": {},
    "sol": {},
    "objet": {}
}

def initialiser_caches(nom_monde):
    """
    Vide les caches en mémoire et les pré-charge avec les données 
    déjà existantes dans la base de données (si elles existent).
    """
    global CACHES
    for table in CACHES.keys():
        CACHES[table].clear()
        
        with sqlite3.connect(f"{nom_monde}.db") as cnx:
            c = cnx.cursor()
            try:
                # On récupère toutes les lignes existantes (ex: SELECT num, biome FROM biome)
                c.execute(f"SELECT num, {table} FROM {table}")
                for row in c.fetchall():
                    id_numerique, nom_caract = row[0], row[1]
                    CACHES[table][nom_caract] = id_numerique
            except sqlite3.Error:
                # Si la table est vide ou n'existe pas encore, on ignore
                pass
                
# =============================================================================
# INITIALISATION DE LA BASE DE DONNÉES
# =============================================================================

def create_db_monde(nom_monde):
    """
    Crée le fichier de base de données SQLite et initialise la structure des tables.
    Utilise des clés étrangères logiques pour lier les index aux libellés.
    """
    with sqlite3.connect(f"{nom_monde}.db") as cnx:
        c = cnx.cursor()
        
        # Table principale contenant les données brutes de chaque case de la grille
        c.execute("""
            CREATE TABLE IF NOT EXISTS tuiles (
                num INTEGER PRIMARY KEY, 
                x INTEGER, 
                y INTEGER, 
                type INTEGER, 
                biome INTEGER, 
                sol INTEGER, 
                objet INTEGER,
                altitude REAL, 
                temperature REAL, 
                humidite REAL,
                quantite INTEGER DEFAULT 0
            )
        """)
        
        # Tables de métadonnées et dictionnaires de correspondance (ID -> Texte)
        c.execute("CREATE TABLE IF NOT EXISTS info (nom TEXT PRIMARY KEY, xmax INTEGER, ymax INTEGER)")
        c.execute("CREATE TABLE IF NOT EXISTS type (num INTEGER PRIMARY KEY AUTOINCREMENT, type TEXT UNIQUE)")
        c.execute("CREATE TABLE IF NOT EXISTS biome (num INTEGER PRIMARY KEY AUTOINCREMENT, biome TEXT UNIQUE)")
        c.execute("CREATE TABLE IF NOT EXISTS sol (num INTEGER PRIMARY KEY AUTOINCREMENT, sol TEXT UNIQUE)")
        c.execute("CREATE TABLE IF NOT EXISTS objet (num INTEGER PRIMARY KEY AUTOINCREMENT, objet TEXT UNIQUE)")
        
        cnx.commit()
    return True

# =============================================================================
# GESTION DES DICTIONNAIRES DE CARACTÉRISTIQUES (Biomes, Sols, Objets)
# =============================================================================

def obtenir_ou_creer_id(nom_monde, table_caract, valeur):
    """
    Cherche l'ID dans le buffer RAM. S'il n'y est pas, il fait l'insertion 
    en base de données, met à jour le buffer et retourne l'ID.
    """
    global CACHES
    
    # 1. Si la valeur est déjà dans notre buffer, on la renvoie immédiatement (0 appel BDD !)
    if valeur in CACHES[table_caract]:
        return CACHES[table_caract][valeur]
    
    # 2. Si elle n'y est pas, on doit l'insérer en base de données
    with sqlite3.connect(f"{nom_monde}.db") as cnx:
        c = cnx.cursor()
        try:
            # Insertion sécurisée
            c.execute(f"INSERT OR IGNORE INTO {table_caract} ({table_caract}) VALUES (?)", (valeur,))
            cnx.commit()
            
            # Récupération de l'ID qui vient d'être généré (ou qui existait déjà)
            c.execute(f"SELECT num FROM {table_caract} WHERE {table_caract} = ?", (valeur,))
            row = c.fetchone()
            if row:
                nuevo_id = row[0]
                # On sauvegarde dans le buffer pour la prochaine tuile !
                CACHES[table_caract][valeur] = nuevo_id
                return nuevo_id
        except sqlite3.Error as e:
            print(f"Erreur d'indexation pour {valeur} dans {table_caract}: {e}")
            
    return -1


# =============================================================================
# ENREGISTREMENT DES DONNÉES DU MONDE
# =============================================================================

def ajoute_info(nom_monde, xmax, ymax):
    """Enregistre les dimensions maximales de la carte générée."""
    with sqlite3.connect(f"{nom_monde}.db") as cnx:
        c = cnx.cursor()
        try:
            c.execute(
                "INSERT OR REPLACE INTO info (nom, xmax, ymax) VALUES (?, ?, ?)", 
                (nom_monde, xmax, ymax)
            )
            cnx.commit()
        except sqlite3.Error as e:
            print(f"Erreur lors de l'insertion des infos mondes pour {nom_monde}: {e}")
    return True


def ajoute_tuile(nom_monde, x, y, typ, biome, sol, objet_data, altitude, temperature, humidite, xmax):
    """
    Enregistre une tuile. Utilise le système de buffers mémoires pour 
    les IDs de caractéristiques afin d'optimiser drastiquement la vitesse.
    """
    # Calcul de l'ID unique de la tuile en 1D
    num_tuile = xmax * y + x

    # Extraction des données de l'objet (gestion de la quantité ajoutée précédemment)
    if isinstance(objet_data, dict):
        nom_objet = objet_data.get("nom", "rien")
        quantite = objet_data.get("quantite", 0)
    else:
        nom_objet = objet_data
        quantite = 0

    # Interrogations des BUFFERS mémoires (Ultra rapide, pas d'appels SQL répétés)
    id_type = obtenir_ou_creer_id(nom_monde, "type", typ)
    id_biome = obtenir_ou_creer_id(nom_monde, "biome", biome)
    id_sol = obtenir_ou_creer_id(nom_monde, "sol", sol)
    id_objet = obtenir_ou_creer_id(nom_monde, "objet", nom_objet)

    # Insertion de la tuile en base de données
    with sqlite3.connect(f"{nom_monde}.db") as cnx:
        c = cnx.cursor()
        try:
            c.execute("""
                INSERT OR REPLACE INTO tuiles (num, x, y, type, biome, sol, objet, altitude, temperature, humidite, quantite) 
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """, (
                num_tuile, x, y, 
                id_type, id_biome, id_sol, id_objet, 
                float(altitude), float(temperature), float(humidite), int(quantite)
            ))
            cnx.commit()
        except sqlite3.Error as e:
            print(f"Erreur insertion tuile {num_tuile}: {e}")
            
    return num_tuile
