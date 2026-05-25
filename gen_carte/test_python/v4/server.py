import numpy as np
import uvicorn
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from world_generator import WorldGenerator
from fastapi.responses import StreamingResponse
from io import BytesIO
from PIL import Image
import io
from fastapi import FastAPI, Response

# 1. On définit les dimensions fixes d'une colonne de chunk
CHUNK_SIZE = 16
CHUNK_HEIGHT = 100  # Ajuste selon la hauteur max de ta V3

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

@app.get("/api/world/settings")
def get_world_settings():
    try:
        # Supposons que ton world_manager connaisse le nombre de chunks max
        # Si tu as des variables globales comme MAP_CHUNKS_X = 100 :
        chunks_x = 20  # À remplacer par ta variable dynamique si elle existe
        chunks_y = 20  # À remplacer par ta variable dynamique si elle existe
        
        return {
            "chunks_count_x": chunks_x,
            "chunks_count_y": chunks_y,
            "chunk_size_meters": 100,
            "world_max_x_meters": chunks_x * 100,
            "world_max_z_meters": chunks_y * 100
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@app.get("/api/world/chunk/{cx}/{cz}")
def get_chunk_v4(cx: int, cz: int):
    # 2. On crée un tableau 3D NumPy vide (0 = AIR)
    # Format de coordonnées locales : [X, Y, Z]
    voxels = np.zeros((CHUNK_SIZE, CHUNK_HEIGHT, CHUNK_SIZE), dtype=np.uint8)
    
    # Coordonnées globales pour correspondre à ton ancienne logique macro/micro
    world_x_offset = cx * CHUNK_SIZE
    world_z_offset = cz * CHUNK_SIZE

    for x in range(CHUNK_SIZE):
        for z in range(CHUNK_SIZE):
            global_x = world_x_offset + x
            global_z = world_z_offset + z
            
            # ICI : Tu appelles ta fonction V3 exacte qui détermine la hauteur
            # à partir de ta macro-carte (world.db) et de ton bruit procédural
            tile_actuelle = generator.world_manager.get_tile(global_x, global_z)
            biome_actuel = tile_actuelle["terrain_type"] if tile_actuelle else "plain"


            hauteur_sol = generator.get_height_at(global_x, global_z,biome_actuel)
            
            # Securité pour ne pas dépasser le plafond du chunk
            hauteur_max = min(int(hauteur_sol), CHUNK_HEIGHT - 1)
            
            # 3. Remplissage 3D : tout ce qui est en dessous ou égal à la hauteur = TERRE (ID 1)
            if hauteur_max >= 0:
                voxels[x, :hauteur_max + 1, z] = 1

    # 4. On aplatit le tableau en continu et on l'envoie en binaire brut
    ALTITUDE_TUNNEL = 1
    HAUTEUR_TUNNEL = 3
    LARGEUR_TUNNEL = 2
    
    # Pour que le tunnel soit continu entre les chunks, on le creuse sur tout l'axe X (0 à 15)
    for x in range(CHUNK_SIZE):
        for y in range(ALTITUDE_TUNNEL, ALTITUDE_TUNNEL + HAUTEUR_TUNNEL):
            # On centre le tunnel au milieu de l'axe Z du chunk (Z entre 6 et 9)
            for z in range(6, 6 + LARGEUR_TUNNEL):
                # On force la valeur à 0 (AIR) pour évider la montagne
                voxels[x, y, z] = 0



    return Response(content=voxels.tobytes(), media_type="application/octet-stream")

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
