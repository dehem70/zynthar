import uvicorn
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from world_generator import WorldGenerator

app = FastAPI(title="Moteur de Monde - API Maquette")

# Activation de CORS pour que la page HTML puisse requêter l'API sans blocage de sécurité
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Initialisation de notre générateur (qui utilise world.db et world_deltas.db)
generator = WorldGenerator("world.db", "world_deltas.db")

@app.get("/api/world/chunk/{x}/{y}")
def get_chunk(x: int, y: int):
    try:
        # 1. On récupère la grille micro 10x10 standard générée par ton système
        micro_grid = generator.get_activated_micro_world(x, y)
        
        tiles_list = []
        for (mx, my), data in micro_grid.items():
            tiles_list.append({
                "mx": mx, 
                "my": my,
                "terrain": data["terrain_type"],
                "walkable": data["is_walkable"],
                "resource": data["local_resource"],
                "abundance": data["resource_abundance"],
                "height": data["height"]
            })

        # =========================================================================
        # SOUDURE GÉOMÉTRIQUE STRICTE (INDEX 10 SIMULÉS SANS INVERSION)
        # =========================================================================
        tile_actuelle = generator.world_manager.get_tile(x, y)
        biome_actuel = tile_actuelle["terrain_type"] if tile_actuelle else "plain"
        
        tile_droite = generator.world_manager.get_tile(x + 1, y)
        biome_droite = tile_droite["terrain_type"] if tile_droite else biome_actuel

        tile_haut = generator.world_manager.get_tile(x, y + 1)
        biome_haut = tile_haut["terrain_type"] if tile_haut else biome_actuel

        # Bord droit (mx = 10) -> Rejoint exactement le mx = 0 du chunk X+1
        for my in range(11):
            global_x = float((x + 1) * 10)
            global_y = float(y * 10 + my) # Mesure absolue continue
            h = generator.get_height_at(global_x, global_y, biome_droite)
            
            tiles_list.append({
                "mx": 10, "my": my, "terrain": biome_droite, "walkable": 1,
                "resource": None, "abundance": 0.0, "height": h
            })

        # Bord haut (my = 10) -> Rejoint exactement le my = 0 du chunk Y+1
        for mx in range(11): 
            global_x = float(x * 10 + mx) # Mesure absolue continue
            global_y = float((y + 1) * 10)
            h = generator.get_height_at(global_x, global_y, biome_haut)

            tiles_list.append({
                "mx": mx, "my": 10, "terrain": biome_haut, "walkable": 1,
                "resource": None, "abundance": 0.0, "height": h
            })            
        # 5. L'EMBALLAGE FINAL : C'est ici que se trouve la clé "tiles" pour le JavaScript !
        return {
            "macro_x": x,
            "macro_y": y,
            "tiles": tiles_list
        }

    except Exception as e:
        # Permet de voir précisément le moindre crash de calcul dans la console Python
        print(f"❌ Crash dans la route API Chunk ({x},{y}) : {e}")
        raise HTTPException(status_code=400, detail=str(e))

@app.get("/api/world/dimensions")
def get_dimensions():
    try:
        """Retourne les limites de la macro-carte (min_x, max_x, min_y, max_y)."""
        limits = generator.world_manager.get_map_dimensions()
        return {
            "min_x": limits[0], "max_x": limits[1],
            "min_y": limits[2], "max_y": limits[3]
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

# =========================================================================
        # SOUDURE UNIFIÉE DES BORDURES (EXTENSIONS À L'INDEX 10)
        # =========================================================================
        tile_actuelle = generator.world_manager.get_tile(x, y)
        biome_actuel = tile_actuelle["terrain_type"] if tile_actuelle else "plain"
        
        tile_droite = generator.world_manager.get_tile(x + 1, y)
        biome_droite = tile_droite["terrain_type"] if tile_droite else biome_actuel

        tile_haut = generator.world_manager.get_tile(x, y + 1)
        biome_haut = tile_haut["terrain_type"] if tile_haut else biome_actuel

        # Bord droit (mx = 10) -> Rejoint le flanc gauche du chunk X+1
        # On va jusqu'à 11 inclus pour couvrir TOUS les coins
        for my in range(11):
            global_x = float((x + 1) * 10)
            global_y = float(y * 10 + min(my, 9))
            h = generator.get_height_at(global_x, global_y, biome_droite)
            
            tiles_list.append({
                "mx": 10, "my": my, "terrain": biome_droite, "walkable": 1,
                "resource": None, "abundance": 0.0, "height": h
            })

        # Bord haut (my = 10) -> Rejoint le flanc bas du chunk Y+1
        # Correction : range(11) pour inclure le pixel de coin manquant !
        for mx in range(11): 
            global_x = float(x * 10 + min(mx, 9))
            global_y = float((y + 1) * 10)
            h = generator.get_height_at(global_x, global_y, biome_haut)

            tiles_list.append({
                "mx": mx, "my": 10, "terrain": biome_haut, "walkable": 1,
                "resource": None, "abundance": 0.0, "height": h
            })

if __name__ == "__main__":
    # Lance le serveur sur http://127.0.0.1:8000
    uvicorn.run(app, host="127.0.0.1", port=8000)
