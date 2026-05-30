import sqlite3
from typing import List, Tuple, Dict, Any

class WorldManager:
    def __init__(self, db_path: str = "world.db"):
        self.db_path = db_path
        self._init_db()

    def _get_connection(self):
        """Ouvre une connexion et active les contraintes de clés étrangères."""
        conn = sqlite3.connect(self.db_path)
        conn.execute("PRAGMA foreign_keys = ON;")
        # Permet d'accéder aux colonnes par leur nom comme un dictionnaire
        conn.row_factory = sqlite3.Row 
        return conn

    def _init_db(self):
        """Initialise la structure de la base de données s'il s'agit d'un nouveau fichier."""
        with self._get_connection() as conn:
            cursor = conn.cursor()
            
            # Table de la géographie
            cursor.execute("""
            CREATE TABLE IF NOT EXISTS map_tiles (
                x INTEGER,
                y INTEGER,
                terrain_type TEXT NOT NULL,
                is_walkable INTEGER DEFAULT 1,
                PRIMARY KEY (x, y)
            );
            """)
            
            # Table des gisements de ressources
            cursor.execute("""
            CREATE TABLE IF NOT EXISTS resource_nodes (
                node_id INTEGER PRIMARY KEY AUTOINCREMENT,
                x INTEGER,
                y INTEGER,
                resource_type TEXT NOT NULL,
                current_abundance REAL NOT NULL,
                max_abundance REAL NOT NULL,
                FOREIGN KEY (x, y) REFERENCES map_tiles(x, y) ON DELETE CASCADE
            );
            """)
            
            # Index pour accélérer les recherches de coordonnées
            cursor.execute("CREATE INDEX IF NOT EXISTS idx_resources_coords ON resource_nodes(x, y);")
            conn.commit()

    # ==========================================
    # 1. FONCTIONS D'ÉCRITURE (Enregistrement)
    # ==========================================

    def save_full_map(self, tiles: List[Tuple[int, int, str, int]]):
        """
        Enregistre ou met à jour l'ensemble des tuiles de la carte.
        Format attendu : [(x, y, 'terrain_type', is_walkable), ...]
        """
        query = """
            INSERT INTO map_tiles (x, y, terrain_type, is_walkable)
            VALUES (?, ?, ?, ?)
            ON CONFLICT(x, y) DO UPDATE SET
                terrain_type = excluded.terrain_type,
                is_walkable = excluded.is_walkable
        """
        with self._get_connection() as conn:
            # executemany est indispensable ici pour insérer des milliers de cases d'un coup
            conn.executemany(query, tiles)
            conn.commit()

    def spawn_resource_nodes(self, nodes: List[Tuple[int, int, str, float, float]]):
        """
        Ajoute des gisements de ressources sur la carte.
        Format attendu : [(x, y, 'resource_type', current_abundance, max_abundance), ...]
        """
        query = """
            INSERT INTO resource_nodes (x, y, resource_type, current_abundance, max_abundance)
            VALUES (?, ?, ?, ?, ?)
        """
        with self._get_connection() as conn:
            conn.executemany(query, nodes)
            conn.commit()

    def update_resource_abundance(self, node_id: int, new_abundance: float) -> bool:
        """
        Met à jour l'abondance d'une ressource (ex: après une collecte par un joueur).
        Si l'abondance tombe à 0, vous pouvez choisir de la laisser à 0 pour une régénération future.
        """
        query = "UPDATE resource_nodes SET current_abundance = ? WHERE node_id = ?"
        with self._get_connection() as conn:
            cursor = conn.execute(query, (max(0.0, new_abundance), node_id))
            conn.commit()
            return cursor.rowcount > 0

    # ==========================================
    # 2. FONCTIONS D'INTERROGATION (Lecture)
    # ==========================================

    def get_tile(self, x: int, y: int) -> Dict[str, Any] | None:
        """Récupère les détails d'une case spécifique et sa ressource éventuelle."""
        query = """
            SELECT t.x, t.y, t.terrain_type, t.is_walkable, r.node_id, r.resource_type, r.current_abundance
            FROM map_tiles t
            LEFT JOIN resource_nodes r ON t.x = r.x AND t.y = r.y
            WHERE t.x = ? AND t.y = ?
        """
        with self._get_connection() as conn:
            row = conn.execute(query, (x, y)).fetchone()
            return dict(row) if row else None

    def get_zone_resources(self, center_x: int, center_y: int, radius: int) -> List[Dict[str, Any]]:
        """
        Trouve toutes les ressources disponibles dans une zone (carrée) autour d'un point.
        Très utile pour limiter ce qu'un joueur voit ou pour détecter les gisements proches.
        """
        query = """
            SELECT node_id, x, y, resource_type, current_abundance, max_abundance
            FROM resource_nodes
            WHERE x BETWEEN ? AND ?
              AND y BETWEEN ? AND ?
              AND current_abundance > 0
        """
        min_x, max_x = center_x - radius, center_x + radius
        min_y, max_y = center_y - radius, center_y + radius
        
        with self._get_connection() as conn:
            rows = conn.execute(query, (min_x, max_x, min_y, max_y)).fetchall()
            return [dict(r) for r in rows]

    def get_map_dimensions(self) -> Tuple[int, int, int, int]:
        """Retourne les limites de la carte : (min_x, max_x, min_y, max_y)"""
        query = "SELECT MIN(x), MAX(x), MIN(y), MAX(y) FROM map_tiles"
        with self._get_connection() as conn:
            row = conn.execute(query).fetchone()
            return tuple(row) if row[0] is not None else (0, 0, 0, 0)
