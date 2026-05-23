import copy
import math
import random
import matplotlib.pyplot as plt
import numpy as np

# Module personnalisé pour la base de données
import gestion_db

# =============================================================================
# CONFIGURATION GLOBALE
# =============================================================================
LARGEUR = 500
LONGUEUR = 1000
NB_ILES = 4
NOM_MONDE = "test"

# Table de permutation globale pour le bruit de Perlin
PERM = []


def initialiser_seed(seed_value=None):
    """Initialise les générateurs aléatoires et la table de permutation pour le bruit."""
    global PERM
    if seed_value is not None:
        random.seed(seed_value)
        np.random.seed(seed_value)

    p = list(range(256))
    random.shuffle(p)
    # Tripler la table pour éviter les dépassements d'index lors des calculs de bruit
    PERM = p * 3


# Initialisation par défaut au démarrage
initialiser_seed()

# =============================================================================
# OUTILS MATHÉMATIQUES & BRUIT PROCÉDURAL
# =============================================================================


def fade(t):
    """Fonction de lissage mathématique (6t^5 - 15t^4 + 10t^3) pour le bruit."""
    return t * t * t * (t * (t * 6 - 15) + 10)


def lerp(a, b, t):
    """Interpolation linéaire simple entre a et b."""
    return a + t * (b - a)


def grad(hash_val, x, y):
    """Calcule le produit scalaire entre un gradient pseudo-aléatoire et la distance."""
    h = hash_val & 3
    if h == 0:
        return x + y
    elif h == 1:
        return -x + y
    elif h == 2:
        return x - y
    else:
        return -x - y


def noise(x, y):
    """Génère une valeur de bruit de Perlin 2D classique (coordonnées continues)."""
    X = int(math.floor(x)) & 255
    Y = int(math.floor(y)) & 255

    xf = x - math.floor(x)
    yf = y - math.floor(y)

    u = fade(xf)
    v = fade(yf)

    aa = PERM[PERM[X] + Y]
    ab = PERM[PERM[X] + Y + 1]
    ba = PERM[PERM[X + 1] + Y]
    bb = PERM[PERM[X + 1] + Y + 1]

    res = lerp(
        lerp(grad(aa, xf, yf), grad(ba, xf - 1, yf), u),
        lerp(grad(ab, xf, yf - 1), grad(bb, xf - 1, yf - 1), u),
        v,
    )
    return res


def fractal_noise(x, y, octaves, persistence, lacunarity=2.0):
    """Superpose plusieurs octaves de bruit de Perlin pour créer un effet fractal."""
    total = 0
    amplitude = 1
    frequence = 1
    max_amplitude = 0

    for _ in range(octaves):
        total += noise(x * frequence, y * frequence) * amplitude
        max_amplitude += amplitude
        amplitude *= persistence
        frequence *= lacunarity

    return total / max_amplitude


# =============================================================================
# GÉNÉRATEURS DE CARTES INDÉPENDANTES
# =============================================================================


def generate_map_fractale(width, height, octaves=6, persistence=0.5):
    """Génère une matrice 2D de relief fractal brut (Heightmap)."""
    initialiser_seed(random.randint(0, 1000000))

    base_scale = random.uniform(0.005, 0.02)
    offset_x = random.uniform(-100000, 100000)
    offset_y = random.uniform(-100000, 100000)

    grid = []
    for y in range(height):
        row = []
        for x in range(width):
            nx = (x + offset_x) * base_scale
            ny = (y + offset_y) * base_scale
            valeur = fractal_noise(nx, ny, octaves, persistence)
            row.append(valeur)
        grid.append(row)

    return grid


def generate_voronoi_map(width, height, num_islands):
    """Génère un masque de Voronoi/Worley pour délimiter les centres de masses terrestres."""
    seeds = np.random.rand(num_islands, 2)
    seeds[:, 0] *= width
    seeds[:, 1] *= height

    voronoi_grid = np.zeros((height, width))
    max_dist = min(height, width) / np.random.randint(3, 6)

    for y in range(height):
        for x in range(width):
            distances = np.sqrt((seeds[:, 0] - x) ** 2 + (seeds[:, 1] - y) ** 2)
            d1 = np.min(distances)
            val = max(0, 1 - (d1 / max_dist))
            voronoi_grid[y, x] = val

    return voronoi_grid


def generer_archipel_avec_fonds(width, height, num_islands, pourcentage_mer_max=0.45):
    """Combine le relief fractal et Voronoi pour créer un archipel avec un taux d'eau calibré."""
    relief_fractal = generate_map_fractale(width, height, octaves=6, persistence=0.5)
    relief_np = np.array(relief_fractal)
    masque_voronoi = generate_voronoi_map(width, height, num_islands)

    intensite_ile = 1.2
    niveau_mer = 0.4

    archipel_complet = relief_np + (masque_voronoi * intensite_ile) - niveau_mer

    # Ajustement automatique pour obtenir précisément la bonne proportion de mer
    valeurs_triees = np.sort(archipel_complet.flatten())
    index_mer = int(len(valeurs_triees) * pourcentage_mer_max)
    niveau_mer_calcule = valeurs_triees[index_mer]

    carte_hauteur_ajustee = archipel_complet - niveau_mer_calcule
    return carte_hauteur_ajustee.tolist()


def generer_carte_temperature(width, height, carte_hauteur):
    """Génère la température dépendante de la latitude et de l'altitude."""
    initialiser_seed(random.randint(0, 10**6))

    off_x = random.uniform(-100000, 100000)
    off_y = random.uniform(-100000, 100000)
    scale_temp = 0.005

    carte_temp = []
    for y in range(height):
        row = []
        for x in range(width):
            nx = (x + off_x) * scale_temp
            ny = (y + off_y) * scale_temp

            t_base = fractal_noise(nx, ny, octaves=3, persistence=0.5)
            hauteur = carte_hauteur[y][x]

            # La température baisse avec l'altitude et l'éloignement de l'équateur (axe central Y)
            t_finale = t_base - (hauteur * 0.3)
            t_finale -= abs(height / 2 - y) / height

            row.append(t_finale)
        carte_temp.append(row)
    return carte_temp


def generer_carte_humidite(width, height):
    """Génère un gradient fractal d'humidité."""
    initialiser_seed(random.randint(0, 10**6))

    scale_humidite = 0.08
    off_x = random.uniform(-100000, 100000)
    off_y = random.uniform(-100000, 100000)

    grid_humidite = []
    for y in range(height):
        row = []
        for x in range(width):
            nx = (x + off_x) * scale_humidite
            ny = (y + off_y) * scale_humidite
            val = fractal_noise(nx, ny, octaves=4, persistence=0.5)
            row.append((val + 1) / 2)
        grid_humidite.append(row)

    return grid_humidite


def generer_carte_vegetation(width, height, carte_humidite, carte_temp):
    """Calcule la densité végétale selon la fertilité météo (humidité & chaleur)."""
    scale_veg = 0.05
    carte_veg = []

    for y in range(height):
        row = []
        for x in range(width):
            densite_brute = (
                fractal_noise(x * scale_veg, y * scale_veg, octaves=4, persistence=0.6) + 1
            ) / 2
            humidite = carte_humidite[y][x]
            temperature = carte_temp[y][x]

            facteur_climat = humidite * max(0, temperature)
            row.append(densite_brute * facteur_climat)
        carte_veg.append(row)
    return carte_veg


def generer_carte_sol(width, height):
    """Définit la composition géologique de base sous forme de bruit continu."""
    scale_geol = 0.01
    carte_sol = []

    for y in range(height):
        row = []
        for x in range(width):
            val = fractal_noise(x * scale_geol, y * scale_geol, octaves=3, persistence=0.5)
            row.append(val)
        carte_sol.append(row)
    return carte_sol


def generer_carte_richesses(width, height):
    """
    Génère les potentiels de ressources par catégorie de rareté avec des
    échelles différentes pour ajuster la taille des gisements.
    """
    # Échelles différenciées pour contrôler l'étalement spatial
    scales = {
        "precieux": 0.007,  # Un zoom un peu moins violent (filons grands et garantis)
        "rare":     0.020,  # Plus condensé
        "commun":   0.040,  # Très fragmenté et petit
    }
    scale_micro = 0.15      # Reste identique pour les pépites isolées
    
    carte_richesses = []
    
    offsets = {
        "precieux": (random.uniform(-100000, 100000), random.uniform(-100000, 100000)),
        "rare":     (random.uniform(-100000, 100000), random.uniform(-100000, 100000)),
        "commun":   (random.uniform(-100000, 100000), random.uniform(-100000, 100000)),
        "micro":    (random.uniform(-100000, 100000), random.uniform(-100000, 100000))
    }
    
    for y in range(height):
        row = []
        for x in range(width):
            # 1. Précieux (Gros filons étendus)
            ox, oy = offsets["precieux"]
            sc = scales["precieux"]
            p_precieux = (fractal_noise((x + ox) * sc, (y + oy) * sc, octaves=2, persistence=0.5) + 1) / 2
            
            # 2. Rare (Filons plus resserrés)
            ox, oy = offsets["rare"]
            sc = scales["rare"]
            p_rare = (fractal_noise((x + ox) * sc, (y + oy) * sc, octaves=2, persistence=0.5) + 1) / 2
            
            # 3. Commun (Petites poches de gisements)
            ox, oy = offsets["commun"]
            sc = scales["commun"]
            p_commun = (fractal_noise((x + ox) * sc, (y + oy) * sc, octaves=2, persistence=0.5) + 1) / 2
            
            # 4. Micro-trésors / Pépites (Points uniques)
            ox, oy = offsets["micro"]
            p_micro = (fractal_noise((x + offsets["micro"][0]) * scale_micro, (y + offsets["micro"][1]) * scale_micro, octaves=1, persistence=0.1) + 1) / 2
            
            row.append((p_precieux, p_rare, p_commun, p_micro))
        carte_richesses.append(row)
        
    # --- Filtre d'exclusion de voisinage pour les pépites ---
    for y in range(height):
        for x in range(width):
            if carte_richesses[y][x][3] > 0.88:
                for dx in [-1, 0, 1]:
                    for dy in [-1, 0, 1]:
                        if dx == 0 and dy == 0: continue
                        nx, ny = x + dx, y + dy
                        if 0 <= nx < width and 0 <= ny < height:
                            actuel = list(carte_richesses[ny][nx])
                            actuel[3] = 0
                            carte_richesses[ny][nx] = tuple(actuel)
                            
    return carte_richesses    

# =============================================================================
# TRAITEMENTS POST-GÉNÉRATION (AUTOMATES & ÉROSION)
# =============================================================================


def generer_carte_binaire(carte_hauteur):
    """Convertit la hauteur en carte binaire : -1 pour l'eau, 1 pour la terre."""
    return np.where(np.array(carte_hauteur) <= 0, -1, 1).tolist()


def combiner_hauteur_binaire(carte_hauteur, carte_binaire):
    """Réapplique le masque binaire lissé sur les valeurs absolues des hauteurs."""
    return (np.abs(np.array(carte_hauteur)) * np.array(carte_binaire)).tolist()


def lisser_carte(grille_binaire, iterations=3):
    """Applique un automate cellulaire type 'Jeu de la vie' pour nettoyer les côtes côtières."""
    height = len(grille_binaire)
    width = len(grille_binaire[0])

    for _ in range(iterations):
        nouvelle_grille = copy.deepcopy(grille_binaire)
        for y in range(1, height - 1):
            for x in range(1, width - 1):
                # Comptage des 8 voisins (Voisinage de Moore)
                voisins_ville = [
                    grille_binaire[y + i][x + j]
                    for i in range(-1, 2)
                    for j in range(-1, 2)
                    if not (i == 0 and j == 0)
                ]
                voisins_terre = voisins_ville.count(1)

                if grille_binaire[y][x] == 1 and voisins_terre < 4:
                    nouvelle_grille[y][x] = -1
                elif grille_binaire[y][x] == -1 and voisins_terre >= 5:
                    nouvelle_grille[y][x] = 1

        grille_binaire = nouvelle_grille

    return grille_binaire


def calculer_pente_et_direction(x, y, carte_hauteur, width, height):
    """Trouve la direction de descente la plus abrupte parmi les 8 voisins."""
    hauteur_actuelle = carte_hauteur[y][x]
    pente_max = 0.0
    meilleur_dx, meilleur_dy = 0, 0

    for dx, dy in [(-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1)]:
        nx, ny = x + dx, y + dy
        if 0 <= nx < width and 0 <= ny < height:
            diff_hauteur = hauteur_actuelle - carte_hauteur[ny][nx]
            distance = (dx**2 + dy**2) ** 0.5
            pente_actuelle = diff_hauteur / distance

            if pente_actuelle > pente_max:
                pente_max = pente_actuelle
                meilleur_dx, meilleur_dy = dx, dy

    return pente_max, meilleur_dx, meilleur_dy


def appliquer_erosion_agents(carte_hauteur, carte_temperature):
    """Simule le ruissellement de gouttes d'eau pour sculpter le relief et tracer les rivières."""
    height = len(carte_hauteur)
    width = len(carte_hauteur[0])

    nb_gouttes = height * width * np.random.randint(1, 3)
    max_vie = 50 * np.random.randint(1, 5)

    carte_riviere = [[0.0 for _ in range(width)] for _ in range(height)]

    capacite_max = 0.1
    taux_erosion = 0.2
    taux_depot = 0.1

    for _ in range(nb_gouttes):
        x = random.randint(1, width - 2)
        y = random.randint(1, height - 2)
        facteur_evaporation = 1 - abs(carte_temperature[y][x]) / 5

        if carte_hauteur[y][x] <= 0:
            continue

        sediment = 0.0
        eau = 1.0

        for _ in range(max_vie):
            carte_riviere[y][x] += eau
            pente, dx, dy = calculer_pente_et_direction(x, y, carte_hauteur, width, height)

            if pente == 0.0:
                carte_hauteur[y][x] += sediment
                break

            capacite_transport = max(pente * eau * capacite_max, 0.01)

            if sediment > capacite_transport:
                quantite_depot = (sediment - capacite_transport) * taux_depot
                carte_hauteur[y][x] += quantite_depot
                sediment -= quantite_depot
            else:
                quantite_erosion = min((capacite_transport - sediment) * taux_erosion, pente)
                carte_hauteur[y][x] -= quantite_erosion
                sediment += quantite_erosion

            x += dx
            y += dy
            eau *= facteur_evaporation

            if carte_hauteur[y][x] <= 0 or eau < 0.01:
                break

    return carte_hauteur, carte_riviere


# =============================================================================
# AGREGATION ET COMPILATION DU MONDE FINAL
# =============================================================================


def generer_monde_final_avec_rivieres(
    width, height, c_hauteur, c_temp, c_humidite, c_veg, c_sol, c_res, c_riviere):
    """Compile toutes les cartes de bruit en une matrice d'entités/dictionnaires 'cases'."""
    monde = []
    seuil_riviere = 25
    hauteur_maxi = 2000 * np.random.randint(1, 4)
    temperature_maxi = 50

    for y in range(height):
        ligne = []
        for x in range(width):
            h = c_hauteur[y][x]
            t = c_temp[y][x]
            m = c_humidite[y][x]
            v = c_veg[y][x]
            s = c_sol[y][x]
            p_precieux, p_rare, p_commun,p_micro = c_res[y][x]
            flux_eau = c_riviere[y][x]

            case = {
                "type": "eau",
                "biome": "océan",
                "sol": "sable",
                "objet": "rien",
                "altitude": h * hauteur_maxi,
                "temperature": t * temperature_maxi,
                "humidite": 100 * m,
            }

            # 1. Gestion des milieux aquatiques
            if h <= 0:
                case["biome"] = "eau_profonde" if h < -0.4 else "eau_cotiere"
            elif flux_eau > seuil_riviere:
                case["type"] = "eau"
                case["biome"] = "rivière"
                case["sol"] = "gravier"

            # 2. Gestion de la terre ferme
            else:
                case["type"] = "terre"

            # Biome climatique (Whittaker modifié)
                if t > 0.6:
                    case["biome"] = "jungle" if m > 0.6 else "désert"
                elif t < 0.2:
                    case["biome"] = "toundra" if m < 0.4 else "taïga"
                elif t < 0:
                    case["biome"] = "glacier"
                else:
                    case["biome"] = "forêt" if m > 0.5 else "plaine"

                # Surcharge par altitude
                if 0 < h < 0.05:
                    case["biome"] = "plage"
                elif h > 0.9:
                    case["biome"] = "pic_enneigé"
                elif h > 0.6:
                    case["biome"] = "montagne_rocheuse"

                # Nature géologique du sol
                if s < -0.3:
                    case["sol"] = "roche"
                elif s > 0.3 or case["biome"] in ["désert", "plage"]:
                    case["sol"] = "sable"
                else:
                    case["sol"] = "terre_fertile"

            # =================================================================
            # CATEGORIE 1 : LES FILONS PRÉCIEUX (Seuil > 0.85)
            # =================================================================
            if p_precieux > 0.7:
                quantite = int((p_precieux - 0.7) * 600) + 30
                
                # --- Déclinaison selon l'environnement ---
                if case["type"] == "eau":
                    if case["biome"] == "eau_profonde":
                        case["objet"] = {"nom": "cristaux_abyssaux", "quantite": quantite}
                    else:
                        case["objet"] = {"nom": "recif_de_perles", "quantite": quantite // 2}
                else: # Sur terre ferme
                    if case["sol"] == "roche" or case["biome"] == "montagne_rocheuse":
                        case["objet"] = {"nom": "filon_or", "quantite": quantite}
                    elif case["biome"] == "désert":
                        case["objet"] = {"nom": "oasis_cachee", "quantite": 1} # Ressource précieuse du désert !

            # =================================================================
            # CATEGORIE 2 : LES FILONS RARES (Seuil > 0.72)
            # =================================================================
            elif p_rare > 0.72 and case["objet"] == "rien":
                quantite = int((p_rare - 0.72) * 400) + 20
                
                # --- Déclinaison selon l'environnement ---
                if case["type"] == "eau":
                    case["objet"] = {"nom": "epave_engloutie", "quantite": quantite} # Contient du loot rare !
                else:
                    if case["sol"] in ["roche", "terre_fertile", "gravier"]:
                        case["objet"] = {"nom": "filon_fer", "quantite": quantite}

            # =================================================================
            # CATEGORIE 3 : LES FILONS COMMUNS (Seuil > 0.60)
            # =================================================================
            elif p_commun > 0.60 and case["objet"] == "rien":
                quantite = int((p_commun - 0.60) * 300) + 15
                
                # --- Déclinaison selon l'environnement ---
                if case["type"] == "eau":
                    case["objet"] = {"nom": "banc_de_poissons", "quantite": quantite * 2} # Ressource commune marine
                else:
                    if case["sol"] == "terre_fertile":
                        case["objet"] = {"nom": "gisement_charbon", "quantite": quantite}
                    elif case["sol"] == "sable":
                        case["objet"] = {"nom": "argile_siliceuse", "quantite": quantite}
            
            # =================================================================
            # CATEGORIE 4 : LES MICRO-RÉCOMPENSES DE SURFACE (Seuil > 0.88)
            # =================================================================
            if case["objet"] == "rien" and p_micro > 0.88:
                quantite = int((p_micro - 0.88) * 40) + 1 # Petites quantités (1 à 5)
                
                # --- Déclinaison contextuelle selon le biome et le sol ---
                if case["type"] == "eau":
                    if case["biome"] == "rivière":
                        case["objet"] = {"nom": "pepite_or", "quantite": quantite} # Orpaillage classique
                    elif case["biome"] == "eau_cotiere":
                        case["objet"] = {"nom": "fragment_corail", "quantite": quantite}
                    else:
                        case["objet"] = {"nom": "algue_rare", "quantite": quantite}
                else: # Sur la terre ferme
                    if case["biome"] == "plage":
                        case["objet"] = {"nom": "coquillage_precieux", "quantite": quantite}
                    elif case["biome"] == "désert":
                        case["objet"] = {"nom": "debris_meteorite", "quantite": quantite}
                    elif case["biome"] in ["montagne_rocheuse", "pic_enneigé"]:
                        case["objet"] = {"nom": "cristal_roche", "quantite": quantite}
                    else:
                        case["objet"] = {"nom": "pepite_cuivre", "quantite": quantite} # Dans les plaines / forêts

            # --- C. VÉGÉTATION DE SURFACE ---
            # Si aucun minerai n'a pris la place
            if case["objet"] == "rien" and v > 0.7:
                if case["biome"] in ["forêt", "taïga"]:
                    case["objet"] = "arbre_dense"
                elif case["biome"] == "jungle":
                    case["objet"] = "palmier"
                elif case["biome"] == "désert":
                    case["objet"] = "cactus"
                elif case["biome"] == "plaine":
                    case["objet"] = "buisson"
            ligne.append(case)
        monde.append(ligne)

    return monde,hauteur_maxi


# =============================================================================
# AFFICHAGE ET EXPORT DE DONNÉES
# =============================================================================


def afficher_monde_couleur(monde_final):
    """Génère une carte visuelle RGB du monde généré et l'enregistre via Matplotlib."""
    height = len(monde_final)
    width = len(monde_final[0])

    image_rgb = np.zeros((height, width, 3))

    couleurs_biomes = {
        "eau_profonde": [0.0, 0.0, 0.4],
        "eau_cotiere": [0.2, 0.5, 0.8],
        "rivière": [0.0, 0.8, 1.0],
        "jungle": [0.0, 0.3, 0.0],
        "forêt": [0.1, 0.5, 0.1],
        "taïga": [0.2, 0.4, 0.3],
        "plaine": [0.5, 0.8, 0.3],
        "désert": [0.9, 0.8, 0.4],
        "plage": [0.9, 0.8, 0.4],
        "toundra": [0.7, 0.8, 0.8],
        "montagne_rocheuse": [0.5, 0.5, 0.5],
        "pic_enneigé": [0.95, 0.95, 0.95],
        "glacier": [0.95, 0.95, 0.95],
    }

    couleurs_objets = {
        # --- Ressources Précieuses (Tons Dorés et Éclatants) ---
        "filon_or": [1.0, 0.84, 0.0],          # Jaune d'or pur
        "cristaux_abyssaux": [0.0, 1.0, 0.8],   # Turquoise néon / Cyan électrique (brille dans le fond)
        "recif_de_perles": [0.9, 0.9, 0.98],    # Blanc nacré / reflets bleutés
        "oasis_cachee": [0.0, 0.9, 0.4],        # Vert émeraude (au milieu du désert)
        
        # --- Ressources Rares (Tons Rouges, Oranges et Violets) ---
        "filon_fer": [0.8, 0.0, 0.0],           # Rouge vif
        "epave_engloutie": [0.6, 0.2, 0.8],     # Violet impérial (contraste fort avec le bleu de la mer)
        
        # --- Ressources Communes (Tons Sombres et Utilitaires) ---
        "gisement_charbon": [0.15, 0.15, 0.15], # Gris anthracite très sombre
        "argile_siliceuse": [0.7, 0.45, 0.3],   # Brun terre cuite / Argile
        "banc_de_poissons": [0.7, 0.9, 1.0],    # Bleu argenté très clair
        
        # --- Micro-Ressources / Pépites de surface ---
        "pepite_or": [0.95, 0.76, 0.2],         # Jaune ambré
        "pepite_cuivre": [0.85, 0.53, 0.4],     # Orange cuivré / Saumon
        "fragment_corail": [1.0, 0.4, 0.4],     # Rose corail éclatant
        "coquillage_precieux": [1.0, 0.9, 0.8], # Blanc cassé / Ivoire
        "debris_meteorite": [0.3, 0.2, 0.2],    # Marron très sombre / Noir brûlé
        "cristal_roche": [0.7, 0.9, 0.9],       # Bleu azur translucide
        "algue_rare": [0.4, 0.7, 0.2]           # Vert lime / Fluide
    }

    for y in range(height):
        for x in range(width):
            case = monde_final[y][x]
            
            # 1. On applique d'abord la couleur du biome par défaut
            couleur = couleurs_biomes.get(case["biome"], [1.0, 0.0, 1.0])

            # 2. EXTRACTION DU NOM DE L'OBJET
            # Si c'est un dictionnaire (filon), on prend la valeur de "nom"
            # Si c'est une chaîne (arbre, buisson, rien), on la garde telle quelle
            obj_data = case["objet"]
            nom_objet = obj_data["nom"] if isinstance(obj_data, dict) else obj_data
            
            # 3. Surcharge visuelle si c'est une ressource connue
            if nom_objet in couleurs_objets:
                couleur = couleurs_objets[nom_objet]
                
            image_rgb[y, x] = couleur


    plt.figure(figsize=(14, 12))
    plt.imshow(image_rgb)
    plt.title("Carte du Monde : Biomes, Rivières et Trésors")
    plt.axis("off")

    nom_fichier = "monde.png"
    plt.savefig(nom_fichier)
    print(f"Fichier sauvegardé : {nom_fichier}")
    plt.close()


def enregistrer_monde(nom, monde):
    # 1. On initialise le buffer avant de commencer la boucle
    gestion_db.initialiser_caches(nom)
    
    height = len(monde)
    width = len(monde[0])
    
    for x in range(width):
        for y in range(height):
            case = monde[y][x]
            # On passe LONGUEUR (qui correspond à xmax) à la fin
            gestion_db.ajoute_tuile(
                nom, x, y, 
                case["type"], case["biome"], case["sol"], case["objet"], 
                case["altitude"], case["temperature"], case["humidite"],
                LONGUEUR 
            )

from PIL import Image

def generer_fichiers_terrain(tableau_monde, largeur, hauteur,hmax):
    # 1. Créer les deux images vides
    img_altitude = Image.new("L", (largeur, hauteur))      # Niveaux de gris (8-bit)
    img_texture = Image.new("RGB", (largeur, hauteur))     # Couleur (Rouge, Vert, Bleu)
    
    pixels_alt = img_altitude.load()
    pixels_tex = img_texture.load()
    
    for y in range(hauteur):
        for x in range(largeur):
            case = tableau_monde[y][x]
            
            # --- 1. Gestion de la hauteur ---
            # Conversion de votre altitude logique (0.0 à 1.0) en octet (0 à 255)
            pixels_alt[x, y] = int(case['altitude']/hmax * 255)
            
            # --- 2. Gestion de la nature du terrain ---
            if case['type'] == 'roche':
                pixels_tex[x, y] = (255, 0, 0)      # Rouge pur pour la roche
            elif case['type'] == 'HERBE':
                pixels_tex[x, y] = (0, 255, 0)      # Vert pur pour l'herbe
            elif case['type'] == 'sable':
                pixels_tex[x, y] = (0, 0, 255)      # Bleu pur pour le sable
            else:
                pixels_tex[x, y] = (0, 0, 0)        # Noir par défaut
                
    # Sauvegarde des fichiers pour votre projet Babylon.js
    img_altitude.save("terrain_heightmap.png")
    img_texture.save("terrain_splatmap.png")
    
# =============================================================================
# POINT D'ENTRÉE PRINCIPAL (MAINEXECUTION)
# =============================================================================
if __name__ == "__main__":
    print("--- Début de la génération procédurale ---")

    # 1. Traitement des structures de relief de base
    print("Génération des cartes de relief...")
    carte_hauteur = generer_archipel_avec_fonds(LONGUEUR, LARGEUR, NB_ILES)
    carte_binaire = generer_carte_binaire(carte_hauteur)
    carte_binaire = lisser_carte(carte_binaire)
    carte_hauteur = combiner_hauteur_binaire(carte_hauteur, carte_binaire)

    # 2. Génération des cartes climatiques
    print("Génération des cartes climatiques...")
    carte_temperature = generer_carte_temperature(LONGUEUR, LARGEUR, carte_hauteur)
    carte_hauteur, carte_riviere = appliquer_erosion_agents(carte_hauteur, carte_temperature)

    # 3. Génération des détails de surface
    print("Génération des cartes de surfaces...")
    carte_ressources = generer_carte_richesses(LONGUEUR, LARGEUR)
    carte_sol = generer_carte_sol(LONGUEUR, LARGEUR)
    carte_humide = generer_carte_humidite(LONGUEUR, LARGEUR)
    carte_vegetation = generer_carte_vegetation(LONGUEUR, LARGEUR, carte_humide, carte_temperature)

    # 4. Compilation du monde sous forme de dictionnaire structuré
    print("Compilation du monde...")
    monde,hmax = generer_monde_final_avec_rivieres(
        LONGUEUR,
        LARGEUR,
        carte_hauteur,
        carte_temperature,
        carte_humide,
        carte_vegetation,
        carte_sol,
        carte_ressources,
        carte_riviere
    )

    # 5. Visualisation graphique
    afficher_monde_couleur(monde)
    generer_fichiers_terrain(monde, LONGUEUR, LARGEUR,hmax)

    # 6. Sauvegarde en Base de Données (Décommenter au besoin)
    #print("Exportation vers la base de données...")
    #gestion_db.create_db_monde(NOM_MONDE)
    #gestion_db.ajoute_info(NOM_MONDE, LONGUEUR, LARGEUR)
    #enregistrer_monde(NOM_MONDE, monde)
    print("--- Génération terminée avec succès ---")
