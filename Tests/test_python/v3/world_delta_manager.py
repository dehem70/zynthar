import sqlite3
from typing import Dict, Any, Tuple, List

class WorldDeltaManager:
    def __init__(self, db_path: str = "world_deltas.db"):
        self.db_path = db_path
        self._init_db()

    def _get_connection(self):
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        return conn

    def _init_db(self):
        """Initialise la table des deltas du monde."""
        with self._get_connection() as conn:
            conn.execute("""
            CREATE TABLE IF NOT EXISTS world_deltas (
                x INTEGER,
                y INTEGER,
                layer TEXT,      -- 'terrain', 'resource', 'building'
                data_key TEXT,   -- 'current_abundance', 'is_walkable', etc.
                data_value TEXT, -- Stockage générique en texte
                PRIMARY KEY (x, y, layer, data_key)
            );
            """)
            conn.commit()

    # ==========================================
    # ÉCRITURE : Enregistrer une modification
    # ==========================================
    
    def set_delta(self, x: int, y: int, layer: str, key: str, value: Any):
        """Enregistre ou met à jour une modification sur une case."""
        query = """
            INSERT INTO world_deltas (x, y, layer, data_key, data_value)
            VALUES (?, ?, ?, ?, ?)
            ON CONFLICT(x, y, layer, data_key) DO UPDATE SET
                data_value = excluded.data_value
        """
        with self._get_connection() as conn:
            conn.execute(query, (x, y, layer, key, str(value)))
            conn.commit()

    # ==========================================
    # LECTURE : Récupérer les modifications
    # ==========================================

    def get_deltas_for_tile(self, x: int, y: int) -> Dict[str, Dict[str, str]]:
        """
        Récupère tous les deltas appliqués à une coordonnée précise.
        Retourne un dictionnaire structuré par couche (layer).
        """
        query = "SELECT layer, data_key, data_value FROM world_deltas WHERE x = ? AND y = ?"
        deltas = {}
        
        with self._get_connection() as conn:
            rows = conn.execute(query, (x, y)).fetchall()
            for row in rows:
                layer = row['layer']
                if layer not in deltas:
                    deltas[layer] = {}
                deltas[layer][row['data_key']] = row['data_value']
                
        return deltas

    def get_zone_deltas(self, min_x: int, max_x: int, min_y: int, max_y: int) -> List[Dict[str, Any]]:
        """Charge d'un coup tous les deltas d'une zone (utile pour l'affichage d'une région)."""
        query = """
            SELECT x, y, layer, data_key, data_value 
            FROM world_deltas 
            WHERE x BETWEEN ? AND ? AND y BETWEEN ? AND ?
        """
        with self._get_connection() as conn:
            rows = conn.execute(query, (min_x, max_x, min_y, max_y)).fetchall()
            return [dict(r) for r in rows]
