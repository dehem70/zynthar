import sqlite3
import os
import time
import random

DB_NAME = "zynthar_macro_chunks.db"
GRID_X = 2000
GRID_Y = 1000
BATCH_SIZE = 50_000

def init_database():
    if os.path.exists(DB_NAME):
        os.remove(DB_NAME)
        
    conn = sqlite3.connect(DB_NAME)
    cursor = conn.cursor()
    
    # Optimisations de performances pour l'écriture de masse
    cursor.execute("PRAGMA journal_mode = WAL;")
    cursor.execute("PRAGMA synchronous = NORMAL;")
    
    # Création de la table selon vos spécifications exactes
    cursor.execute("""
    CREATE TABLE IF NOT EXISTS macro_chunks (
        chunk_x INTEGER NOT NULL,
        chunk_y INTEGER NOT NULL,
        biome_type INTEGER NOT NULL,
        temperature REAL,
        humidity REAL NOT NULL,
        max_elevation REAL,
        PRIMARY KEY (chunk_x, chunk_y)
    );
    """)
    
    conn.commit()
    return conn, cursor

def generate_zynthar_world(conn, cursor):
    print(f"Génération du squelette du monde de Zynthar ({GRID_X}x{GRID_Y} macro-chunks)...")
    start_time = time.time()
    
    count = 0
    batch = []
    
    # Parcours de la grille de 1000km x 500km (par pas de 500m)
    for x in range(GRID_X):
        for y in range(GRID_Y):
            
            # Simulation de bruit/données pour le cadrage
            biome_type = (x // 100 + y // 100) % 6  # 6 biomes différents
            temperature = round(20.0 + 15.0 * (y / GRID_Y), 2) # Simulation de gradient nord/sud
            humidity = round(random.uniform(0.0, 100.0), 2)    # Humidité en %
            max_elevation = round(random.uniform(0.0, 3000.0), 1) # Élévation max (0 à 3000m)
            
            batch.append((x, y, biome_type, temperature, humidity, max_elevation))
            count += 1
            
            if len(batch) == BATCH_SIZE:
                cursor.execute("BEGIN TRANSACTION;")
                cursor.executemany("""
                    INSERT INTO macro_chunks (chunk_x, chunk_y, biome_type, temperature, humidity, max_elevation)
                    VALUES (?, ?, ?, ?, ?, ?);
                """, batch)
                conn.commit()
                batch = []
                print(f"Progression : {count:,} / 2,000,000 macro-chunks insérés...")

    # Dernier batch
    if batch:
        cursor.execute("BEGIN TRANSACTION;")
        cursor.executemany("""
            INSERT INTO macro_chunks (chunk_x, chunk_y, biome_type, temperature, humidity, max_elevation)
            VALUES (?, ?, ?, ?, ?, ?);
        """, batch)
        conn.commit()

    end_time = time.time()
    print(f"\n Monde généré en {end_time - start_time:.2f} secondes.")

def test_query(cursor):
    print("\n--- Test de récupération par le serveur ---")
    # Le joueur est au milieu du monde, le serveur cherche le macro-chunk associé
    start = time.time()
    chunk_x=int(random.uniform(0.0, 2000))
    chunk_y=int(random.uniform(0.0, 1000))
    cursor.execute("SELECT * FROM macro_chunks WHERE chunk_x = "+str(chunk_x)+" AND chunk_y = "+str(chunk_y)+";")
    res = cursor.fetchone()
    end = time.time()
    print(f"Données du macro-chunk (",chunk_x,",",chunk_y,") : {res}")
    print(f"Temps d'accès : {(end - start) * 1000:.4f} ms")

def benchmark_aleatoire(cursor, nombre_essais=1000):
    print(f"Calcul du temps réel sur {nombre_essais} requêtes aléatoires (sans affichage)...")
    
    # On prépare 1000 coordonnées aléatoires à l'avance pour ne pas fausser la mesure
    coordonnees = [
        (random.randint(0, 1999), random.randint(0, 999)) 
        for _ in range(nombre_essais)
    ]
    
    # --- DEBUT DE LA MESURE STRICTE ---
    start_total = time.time()
    
    for x, y in coordonnees:
        cursor.execute("SELECT * FROM macro_chunks WHERE chunk_x = ? AND chunk_y = ?;", (x, y))
        res = cursor.fetchone() # On récupère la ligne en mémoire, mais on ne l'affiche pas
        
    end_total = time.time()
    # --- FIN DE LA MESURE STRICTE ---
    
    temps_total_ms = (end_total - start_total) * 1000
    temps_moyen_ms = temps_total_ms / nombre_essais
    
    print("\n--- RÉSULTATS DU BENCHMARK ---")
    print(f"Temps total pour {nombre_essais} requêtes : {temps_total_ms:.2f} ms")
    print(f"Temps moyen par macro-chunk : {temps_moyen_ms:.4f} ms")


if __name__ == "__main__":
    connection, db_cursor = init_database()
    generate_zynthar_world(connection, db_cursor)
    benchmark_aleatoire(db_cursor, 1000)
    file_size_mb = os.path.getsize(DB_NAME) / (1024 * 1024)
    print(f"Taille du squelette du monde sur disque : {file_size_mb:.2f} Mo")
    connection.close()
