import random
from typing import Dict, List, Any, Tuple
from noise import pnoise2 # Importation du bruit de Perlin 2D
from world_manager import WorldManager
from world_delta_manager import WorldDeltaManager

class WorldGenerator:
    def __init__(self, world_db_path: str = "world.db", delta_db_path: str = "world_deltas.db"):
        self.world_manager = WorldManager(world_db_path)
        self.delta_manager = WorldDeltaManager(delta_db_path)
        # Graine globale du monde pour le bruit procédural
        self.world_seed = 42 

    # ==========================================
    # NIVEAU 1 : Génération Cohérente de la Macro-Carte
    # ==========================================
    def generate_and_save_macro_world(self, width: int = 100, height: int = 100):
        print(f"Génération d'un monde macro cohérent ({width}x{height}) via Perlin Noise...")
        macro_tiles = []
        resource_nodes = []

        # Échelle du bruit : plus la valeur est grande, plus les biomes sont grands et étalés
        scale = 20.0 

        for x in range(width):
            for y in range(height):
                # Évaluation du bruit entre -1.0 et 1.0
                noise_val = pnoise2(x / scale, y / scale, octaves=3, persistence=0.5, lacunarity=2.0, base=self.world_seed)

                # Détermination du biome selon la valeur du bruit (continuité assurée !)
                if noise_val < -0.2:
                    biome = "ocean"
                elif noise_val < 0.1:
                    biome = "plain"
                elif noise_val < 0.4:
                    biome = "forest"
                else:
                    biome = "mountain"

                is_walkable = 0 if biome == "ocean" else 1
                macro_tiles.append((x, y, biome, is_walkable))

                # Placement des ressources indexé sur le bruit
                if biome == "mountain" and noise_val > 0.5:
                    resource_nodes.append((x, y, "iron_vein", 500.0, 500.0))
                elif biome == "forest" and noise_val > 0.25:
                    resource_nodes.append((x, y, "dense_forest", 300.0, 300.0))

        self.world_manager.save_full_map(macro_tiles)
        self.world_manager.spawn_resource_nodes(resource_nodes)
        print("Macro-monde de Perlin enregistré en BDD.")

    # ==========================================
    # NIVEAU 2 : Génération Micro Dynamique Cohérente
    # ==========================================
    def get_height_at(self, global_x: float, global_y: float, macro_biome: str) -> float:
        """
        Formule mathématique UNIQUE pour la Heightmap globale.
        Garantit la continuité absolue des calculs peu importe le chunk.
        """
        from noise import pnoise2
        
        frequency_collines = 25.0
        frequency_petits_details = 6.0

        # 1. Couches de bruit superposées
        noise_base = pnoise2(global_x / frequency_collines, 
                             global_y / frequency_collines, 
                             octaves=3, 
                             base=self.world_seed)

        noise_details = pnoise2(global_x / frequency_petits_details, 
                                global_y / frequency_petits_details, 
                                octaves=1, 
                                base=self.world_seed + 1)

        total_noise = (noise_base * 0.7) + (noise_details * 0.3)

        # 2. Paramètres des biomes (Doivent être identiques partout)
        if macro_biome == "mountain":
            base_height = 10.0
            amplitude = 55.0
        elif macro_biome == "forest":
            base_height = 5.0
            amplitude = 22
        elif macro_biome == "ocean":
            base_height = 0.5
            amplitude = 1.5
        else: # plain
            base_height = 3.0
            amplitude = 14.0

        calculated_height = base_height + (total_noise * amplitude)
        return round(max(0.2, calculated_height), 2)


    def _generate_micro_grid(self, macro_x: int, macro_y: int, macro_biome: str) -> Dict[Tuple[int, int], Dict[str, Any]]:
        """Génère la grille locale 10x10 standard."""
        micro_grid = {}

        for mx in range(10):
            for my in range(10):
                global_x = float(macro_x * 10 + mx)
                global_y = float(macro_y * 10 + my)

                # Appel à la fonction centrale
                height = self.get_height_at(global_x, global_y, macro_biome)

                micro_grid[(mx, my)] = {
                    "terrain_type": macro_biome,
                    "is_walkable": 0 if macro_biome == "ocean" else 1,
                    "height": height,
                    "local_resource": None,
                    "resource_abundance": 0.0
                }

        return micro_grid

    def get_activated_micro_world(self, macro_x: int, macro_y: int) -> Dict[Tuple[int, int], Dict[str, Any]]:
        macro_tile = self.world_manager.get_tile(macro_x, macro_y)
        if not macro_tile:
            # Si le joueur sort de la zone générée en BDD, on renvoie une plaine par défaut
            macro_tile = {"terrain_type": "plain", "is_walkable": 1}

        micro_grid = self._generate_micro_grid(macro_x, macro_y, macro_tile["terrain_type"])

        # Application des deltas persistants (inchangé)
        deltas = self.delta_manager.get_zone_deltas(macro_x, macro_x, macro_y, macro_y)
        for delta in deltas:
            parts = delta["data_key"].split("_")
            if len(parts) == 3:
                mx, my, prop = int(parts[0]), int(parts[1]), parts[2]
                if (mx, my) in micro_grid:
                    if prop == "abundance":
                        micro_grid[(mx, my)]["resource_abundance"] = float(delta["data_value"])
                    elif prop == "terrain":
                        micro_grid[(mx, my)]["terrain_type"] = delta["data_value"]

        return micro_grid
if __name__ == "__main__":
    import os

    # 1. Optionnel : On supprime l'ancienne base pour forcer une génération propre avec Perlin
    # Si tu veux conserver tes données d'une fois sur l'autre, commente ces lignes.
    if os.path.exists("world.db"):
        try:
            os.remove("world.db")
            print("Ancienne base 'world.db' supprimée pour reset.")
        except PermissionError:
            print("Impossible de supprimer 'world.db' : le fichier est utilisé par le serveur API. Coupe-le puis réessaie.")

    # 2. Instanciation du générateur
    generator = WorldGenerator(world_db_path="world.db", delta_db_path="world_deltas.db")
    
    # 3. Génération d'une carte de 100x100 macro-cases (ce qui fait déjà 100 km x 100 km !)
    # C'est parfait pour tester le streaming de chunks dans ta maquette Babylon.js
    generator.generate_and_save_macro_world(width=100, height=100)
    
    print("\n--- TEST TECHNIQUE LOCAL ---")
    # On vérifie qu'on arrive bien à lire la micro-grille de la case de départ (5,5)
    test_chunk = generator.get_activated_micro_world(5, 5)
    print(f"Nombre de micro-cases générées dans le chunk (5,5) : {len(test_chunk)}")
    print(f"État initial de la micro-case centrale (5,5) : {test_chunk[(5,5)]}")
