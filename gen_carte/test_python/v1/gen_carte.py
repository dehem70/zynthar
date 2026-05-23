import random

import math

import matplotlib.pyplot as plt

import numpy as np

import copy
import gestion_db

largeur=500
longueur=1000
nb_iles=4
nom="test"

from matplotlib.colors import LightSource

def generer_monde_final_avec_rivieres(width, height, c_hauteur, c_temp, c_humidite, c_veg, c_sol, c_res, c_riviere):
    monde = []
    
    # Seuil pour définir à partir de combien de passages d'eau on crée une rivière
    seuil_riviere = 25 
    hauteur_maxi=2000*np.random.randint(4)
    temperature_maxi=50

    for y in range(height):
        ligne = []
        for x in range(width):
            h = c_hauteur[y][x]
            t = c_temp[y][x]
            m = c_humidite[y][x]
            v = c_veg[y][x]
            s = c_sol[y][x]
            r = c_res[y][x]
            flux_eau = c_riviere[y][x] # NOUVELLE CARTE
            
            case = {"type": "eau", "biome": "océan", "sol": "sable", "objet": "rien", "altitude":0 , "temperature":0, "humidite":0}
            
            case['temperature']=t*temperature_maxi
            case['altitude']=h*hauteur_maxi
            case['humidite']=100*m
            
            # 1. EST-CE DE L'OCÉAN ?
            if h <= 0:
                case["biome"] = "eau_profonde" if h < -0.4 else "eau_cotiere"
                
            # 2. EST-CE UNE RIVIÈRE ? (L'eau douce à l'intérieur des terres)
            elif flux_eau > seuil_riviere:
                case["type"] = "eau"
                case["biome"] = "rivière"
                case["sol"] = "gravier" # Le fond de la rivière
                
            # 3. SINON, C'EST DE LA TERRE FERME
            else:
                case["type"] = "terre"

            # 2. DÉFINITION DU BIOME CLIMATIQUE (Diagramme de Whittaker)
                if t > 0.6:
                    case["biome"] = "jungle" if m > 0.6 else "désert"
                elif t < 0.2:
                    case["biome"] = "toundra" if m < 0.4 else "taïga"
                elif t < 0 :
                    case["biome"] = "glacier"
                else:
                    case["biome"] = "forêt" if m > 0.5 else "plaine"
                
                # 3. SURCHARGE PAR L'ALTITUDE (La hauteur modifie le climat local)
                if h>0 and h<0.05:
                    case["biome"] = "plage"
                if h > 0.9:
                    case["biome"] = "pic_enneigé"
                elif h > 0.6:
                    case["biome"] = "montagne_rocheuse"
                    
                # 4. NATURE DU SOL (Indépendant du biome, ajoute de la variété)
                if s < -0.3:
                    case["sol"] = "roche"
                elif s > 0.3 or (case["biome"] == "désert") or (case["biome"] == "plage"):
                    # On force le sable dans le désert ou sur les côtes (plages)
                    case["sol"] = "sable"
                else:
                    case["sol"] = "terre_fertile"
                    
                # 5. PEUPLEMENT DES OBJETS (Végétation et Ressources)
                # Les ressources rares (trésors/minerais) ont la priorité
                if r > 0.95: 
                    case["objet"] = "filon_or" if case["sol"] == "roche" else ""
                elif r > 0.9:
                    case["objet"] = "filon_fer" if case["sol"] == "roche" else ""
                # Sinon on place la végétation
                elif v > 0.7:
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
        
    return monde

def afficher_monde_couleur(monde_final):
    """
    Affiche la matrice du monde final en utilisant des codes couleurs RGB 
    pour chaque type de biome, ainsi que pour les rivières et les objets rares.
    """
    height = len(monde_final)
    width = len(monde_final[0])
    
    # Création d'une image vide avec 3 canaux (Rouge, Vert, Bleu)
    image_rgb = np.zeros((height, width, 3))
    
    # Dictionnaire des couleurs (RGB normalisé de 0.0 à 1.0)
    couleurs_biomes = {
        "eau_profonde": [0.0, 0.0, 0.4],       # Bleu marine
        "eau_cotiere": [0.2, 0.5, 0.8],        # Bleu clair
        "rivière": [0.0, 0.8, 1.0],            # Cyan vif
        "jungle": [0.0, 0.3, 0.0],             # Vert très sombre
        "forêt": [0.1, 0.5, 0.1],              # Vert classique
        "taïga": [0.2, 0.4, 0.3],              # Vert-gris
        "plaine": [0.5, 0.8, 0.3],             # Vert clair
        "désert": [0.9, 0.8, 0.4],             # Jaune sable
        "plage": [0.9, 0.8, 0.4],
        "toundra": [0.7, 0.8, 0.8],            # Gris bleuté clair
        "montagne_rocheuse": [0.5, 0.5, 0.5],  # Gris moyen
        "pic_enneigé": [0.95, 0.95, 0.95],      # Blanc
        "glacier": [0.95, 0.95, 0.95]      # Blanc
        
    }
    
    # Couleurs de surbrillance pour les ressources et trésors
    couleurs_objets = {
        "filon_or": [1.0, 0.84, 0.0],    # Or / Jaune éclatant
        "filon_fer": [0.8, 0.0, 0.0] # Rouge vif
    }    
    # Parcours de chaque case pour lui attribuer une couleur
    for y in range(height):
        for x in range(width):
            case = monde_final[y][x]
            biome = case["biome"]
            objet = case["objet"]
            
            # 1. On applique d'abord la couleur du biome
            # (Si le biome n'est pas dans le dictionnaire, on met du magenta pour repérer l'erreur)
            couleur = couleurs_biomes.get(biome, [1.0, 0.0, 1.0])
            
            # 2. Surcharge visuelle s'il y a un point d'intérêt
            if objet in couleurs_objets:
                couleur = couleurs_objets[objet]
                
            image_rgb[y, x] = couleur

    # Affichage avec Matplotlib
    plt.figure(figsize=(14, 12))
    plt.imshow(image_rgb)
    plt.title("Carte du Monde : Biomes, Rivières et Trésors")
    plt.axis('off') # On cache les axes (0 à width/height) pour un rendu "carte"
    nom_fichier="monde"    

    plt.savefig(nom_fichier)
    print(f"Fichier sauvegardé : {nom_fichier}")
    plt.close() # Ferme la figure pour libérer la mémoire

def calculer_pente_et_direction(x, y, carte_hauteur, width, height):
    """
    Trouve le voisin le plus bas autour de (x, y) pour déterminer la pente et la direction.
    """
    hauteur_actuelle = carte_hauteur[y][x]
    pente_max = 0.0
    meilleur_dx, meilleur_dy = 0, 0

    # Vérification du voisinage de Moore (8 voisins)
    for dx, dy in [(-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1)]:
        nx, ny = x + dx, y + dy
        
        # S'assurer que l'on reste dans les limites terrestres de la carte
        if 0 <= nx < width and 0 <= ny < height:
            diff_hauteur = hauteur_actuelle - carte_hauteur[ny][nx]
            # Calcul de la distance au voisin (diagonale = sqrt(2), orthogonale = 1)
            distance = (dx**2 + dy**2)**0.5
            pente_actuelle = diff_hauteur / distance
            
            # On cherche la descente la plus abrupte
            if pente_actuelle > pente_max:
                pente_max = pente_actuelle
                meilleur_dx, meilleur_dy = dx, dy
                
    return pente_max, meilleur_dx, meilleur_dy

def generer_carte_binaire(carte_hauteur):

    carte_binaire=[]
    height = len(carte_hauteur)
    width = len(carte_hauteur[0])
    for y in range(height):
        row = []
        for x in range(width):    
           if carte_hauteur[y][x]<=0:
               row.append(-1)
           else:
               row.append(1)
        carte_binaire.append(row)
    return carte_binaire    

def combiner_hauteur_binaire(carte_hauteur,carte_binaire):

    carte_combine=[]
    height = len(carte_hauteur)
    width = len(carte_hauteur[0])
    for y in range(height):
        row = []
        for x in range(width):
           row.append(abs(carte_hauteur[y][x])*carte_binaire[y][x])
        carte_combine.append(row)
    return carte_combine    


def appliquer_erosion_agents(carte_hauteur,carte_temperature):
    """
    Simule l'érosion hydraulique et génère simultanément la carte des rivières.
    Retourne la carte de hauteur modifiée ET la carte d'accumulation d'eau.
    """
    height = len(carte_hauteur)
    width = len(carte_hauteur[0])
    nb_gouttes=height*width*np.random.randint(3)
    max_vie=50*np.random.randint(5)
    
    # NOUVEAU : Initialisation de la carte d'accumulation d'eau
    carte_riviere = [[0.0 for _ in range(width)] for _ in range(height)]
    
    capacite_max = 0.1
    taux_erosion = 0.2
    taux_depot = 0.1
    
    for _ in range(nb_gouttes):
        x = random.randint(1, width - 2)
        y = random.randint(1, height - 2)
        facteur_evaporation = 1-abs(carte_temperature[y][x])/5
        
        if carte_hauteur[y][x] <= 0:
            continue
            
        sediment = 0.0
        eau = 1.0
        
        for pas in range(max_vie):
            # NOUVEAU : On enregistre le passage de l'eau sur cette case.
            # Plus il passe d'eau, plus la valeur de la rivière sera grande.
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

def generer_carte_ressources(width, height, seuil_rarete=0.8):
    # Utilisation d'une échelle très fine pour des gisements localisés
    scale_ressources = 0.03 
    carte_res = []
    seed_res = random.randint(0, 10**6)
    
    for y in range(height):
        row = []
        for x in range(width):
            # Bruit à haute fréquence (peu d'octaves pour éviter l'étalement)
            val = fractal_noise(x * scale_ressources, y * scale_ressources, octaves=2, persistence=0.5)
            
            # Normalisation et seuillage : seule une petite fraction > seuil_rarete
            val_norm = (val + 1) / 2
            if val_norm > seuil_rarete:
                row.append(val_norm) # Présence d'une ressource
            else:
                row.append(0) # Zone vide
        carte_res.append(row)
    return carte_res

def generer_carte_sol(width, height):
    # Échelle large pour des zones géologiques étendues
    scale_geol = 0.01 
    carte_sol = []
    
    for y in range(height):
        row = []
        for x in range(width):
            val = fractal_noise(x * scale_geol, y * scale_geol, octaves=3, persistence=0.5)
            # On mappe la valeur (-1 à 1) sur des indices de matériaux
            # ex: < -0.3: Roche, -0.3 à 0.3: Terre, > 0.3: Gravier
            row.append(val) 
        carte_sol.append(row)
    return carte_sol


def generer_carte_vegetation(width, height, carte_humidite, carte_temp):
    scale_veg = 0.05
    carte_veg = []
    
    for y in range(height):
        row = []
        for x in range(width):
            # 1. Bruit de densité de base
            densite_brute = (fractal_noise(x * scale_veg, y * scale_veg, octaves=4, persistence=0.6) + 1) / 2
            
            # 2. Contrainte climatique : on ne pousse que si c'est assez humide
            humidite = carte_humidite[y][x]
            temperature = carte_temp[y][x]
            
            # Facteur de croissance : plus c'est humide et chaud, plus c'est dense
            facteur_climat = humidite * max(0, temperature)
            
            # Résultat final : densité de flore à ce point
            row.append(densite_brute * facteur_climat)
        carte_veg.append(row)
    return carte_veg

def generer_carte_humidite(width, height):
    """
    Génère une carte d'humidité basée sur un bruit fractal avec une seed aléatoire.
    """
    # 1. Génération d'une seed propre à l'humidité
    seed_humidite = random.randint(0, 10**6)
    random.seed(seed_humidite)
    p=[]
    for i in range(1,255):
       p.append(i)
    random.shuffle(p)
    perm = p + p + p

    # 2. Paramètres : une échelle intermédiaire permet d'avoir 
    # des zones de pluie réalistes (ni trop petites, ni trop vastes)
    scale_humidite = 0.08 
    off_x = random.uniform(-100000, 100000)
    off_y = random.uniform(-100000, 100000)

    grid_humidite = []
    for y in range(height):
        row = []
        for x in range(width):
            nx = (x + off_x) * scale_humidite
            ny = (y + off_y) * scale_humidite
            
            # On utilise souvent moins d'octaves pour l'humidité (climat plus doux)
            val = fractal_noise(nx, ny, octaves=4, persistence=0.5)
            
            # Normalisation entre 0 (sec) et 1 (très humide)
            row.append((val + 1) / 2)
        grid_humidite.append(row)
        
    return grid_humidite

def generer_carte_temperature(width, height, carte_hauteur):
    # On utilise une graine différente pour que le "chaud" ne soit pas au même endroit que le "haut"
    # 1. Génération aléatoire de la seed à l'intérieur de la fonction [1], [5]
    seed_temp = random.randint(0, 10**6)
    random.seed(seed_temp)
    p=[]
    for i in range(1,255):
       p.append(i)
    random.shuffle(p)
    perm = p + p + p
    # Paramètres de décalage aléatoires pour cette carte
    off_x = random.uniform(-100000, 100000)
    off_y = random.uniform(-100000, 100000)
    
    # Échelle plus grande pour des transitions de température douces
    scale_temp = 0.005 

    carte_temp = []
    for y in range(height):
        row = []
        for x in range(width):
            nx = (x + off_x) * scale_temp
            ny = (y + off_y) * scale_temp
            
            # 1. Bruit de base pour la température (entre -1 et 1)
            t_base = fractal_noise(nx, ny, octaves=3, persistence=0.5)
            
            # 2. Influence de l'altitude : on récupère la hauteur correspondante
            hauteur = carte_hauteur[y][x]
            
            # Formule : Température diminue avec l'altitude
            # On pondère l'influence de la hauteur (ex: 30%)
            t_finale = t_base - (hauteur * 0.3)
            
            t_finale -= abs(height/2-y)/height
            
            row.append(t_finale)
        carte_temp.append(row)
    return carte_temp

def generer_archipel_avec_fonds(width, height,num_islands, pourcentage_mer_max=0.45):
    """
    Génère une carte de hauteur où le niveau de la mer est ajusté
    pour garantir une proportion exacte d'eau (par défaut 45%).
    """
    # 1. Paramètres de base du bruit
    scale = 0.01
    off_x = random.uniform(-100000, 100000)
    off_y = random.uniform(-100000, 100000)

    p=[]
    for i in range(1,255):
       p.append(i)
    random.shuffle(p)
    global perm
    perm = p + p + p
    
    relief_fractal = generate_map_fractale(width, height, octaves=6, persistence=0.5)
    relief_np = np.array(relief_fractal)
    # 2. Génération des valeurs brutes
    masque_voronoi = generate_voronoi_map(width, height, num_islands)
    
    # 3. Fusion par décalage (et non par multiplication simple)
    # On ajoute le masque pour faire émerger les îles tout en gardant 
    # le relief sous-marin là où le masque est faible.
    intensite_ile = 1.2  # Force avec laquelle les îles sortent de l'eau
    niveau_mer = 0.4     # Ajuste la quantité globale d'eau sur la carte
    
    archipel_complet = relief_np + (masque_voronoi * intensite_ile) - niveau_mer
            
    hauteurs_np = np.array(archipel_complet)
    
    # 3. CALCUL AUTOMATIQUE DU NIVEAU DE LA MER
    # On aplatit la grille en une liste 1D et on la trie de la plus basse à la plus haute valeur
    valeurs_triees = np.sort(hauteurs_np.flatten())
    
    # On cherche l'index exact qui correspond au pourcentage de mer désiré
    index_mer = int(len(valeurs_triees) * pourcentage_mer_max)
    
    # La valeur à cet index devient notre nouveau niveau de la mer "zéro"
    niveau_mer_calcule = valeurs_triees[index_mer]
    
    # 4. Ajustement final de la carte
    # On abaisse ou on remonte tout le terrain pour que le niveau calculé devienne 0.
    # Ainsi, exactement "pourcentage_mer_max" des cases seront <= 0 !
    carte_hauteur_ajustee = hauteurs_np - niveau_mer_calcule
    
    return carte_hauteur_ajustee.tolist()


def lisser_carte(grille_binaire, iterations=3):
    """
    Applique un automate cellulaire pour lisser les côtes.
    :param grille_binaire: Une matrice de -1 (mer) et 1 (terre)
    :param iterations: Nombre de passages (2-3 suffisent généralement)
    """
    height = len(grille_binaire)
    width = len(grille_binaire[0])
    
    for _ in range(iterations):
        # Utilisation d'un buffer pour ne pas modifier la grille en cours de calcul [6]
        nouvelle_grille = copy.deepcopy(grille_binaire)
        
        for y in range(1, height - 1):
            for x in range(1, width - 1):
                # Compter les voisins vivants (Terre) dans le voisinage de Moore (8 voisins)
                voisins_terre = 0
                for i in range(-1, 2):
                    for j in range(-1, 2):
                        if i == 0 and j == 0: continue
                        if grille_binaire[y+i][x+j] == 1:
                            voisins_terre += 1
                
                # RÈGLES DE TRANSITION [2, 4]
                # Si la cellule est terre et a moins de 4 voisins terre : elle devient mer
                if grille_binaire[y][x] == 1 and voisins_terre < 4:
                    nouvelle_grille[y][x] = -1
                # Si la cellule est mer et a 5 voisins terre ou plus : elle devient terre
                elif grille_binaire[y][x] == -1 and voisins_terre >= 5:
                    nouvelle_grille[y][x] = 1
                    
        grille_binaire = nouvelle_grille
        
    return grille_binaire
def generate_voronoi_map(width, height, num_islands):
    # 1. Placer des graines (centres des futures îles) aléatoirement
    seeds = np.random.rand(num_islands, 2)
    seeds[:, 0] *= width
    seeds[:, 1] *= height
    
    voronoi_grid = np.zeros((height, width))
    
    for y in range(height):
        for x in range(width):
            # 2. Trouver la distance à la graine la plus proche (d1)
            # C'est la base du bruit de Worley/Voronoi
            distances = np.sqrt((seeds[:, 0] - x)**2 + (seeds[:, 1] - y)**2)
            d1 = np.min(distances)
            
            # Normalisation (plus on est loin de la graine, plus la valeur diminue)
            # On crée ainsi des "dômes" centrés sur chaque graine
            max_dist = min(height,width) / np.random.randint(3,6) # Rayon d'influence arbitraire
            val = max(0, 1 - (d1 / max_dist))
            voronoi_grid[y, x] = val
            
    return voronoi_grid


def fade(t):
    # Fonction de lissage (6t^5 - 15t^4 + 10t^3) [6]
    return t * t * t * (t * (t * 6 - 15) + 10)

def lerp(a, b, t):
    # Interpolation linéaire entre a et b [141, 144]
    return a + t * (b - a)

def grad(hash, x, y):
    # Calcule le produit scalaire entre un gradient et le vecteur distance [192]
    # En 2D, nous utilisons 4 directions cardinales (±1, ±1) ou (±1, 0), (0, ±1) [4, 125]
    h = hash & 3 
    if h == 0:
        return x + y 
    elif h == 1:
        return -x + y 
    elif h == 2:
        return x - y
    else:
        return -x - y

def noise(x, y):
    # Étape 1 : Trouver la cellule de la grille (coordonnées entières) [33]
    X = int(math.floor(x)) & 255
    Y = int(math.floor(y)) & 255

    # Coordonnées relatives dans la cellule [104] [33, 81]
    xf = x - math.floor(x)
    yf = y - math.floor(y)

    # Étape 2 : Calculer les poids d'interpolation [5, 51]
    u = fade(xf)
    v = fade(yf)

    # Étape 3 : Hacher les coordonnées des 4 sommets de la cellule [4]
    aa = perm[perm[X] + Y]
    ab = perm[perm[X] + Y + 1]
    ba = perm[perm[X + 1] + Y]
    bb = perm[perm[X + 1] + Y + 1]

    # Étape 4 : Produits scalaires et interpolation [1, 82, 192]
    # On interpole entre les résultats des produits scalaires des 4 sommets
    res = lerp(
        lerp(grad(aa, xf, yf), grad(ba, xf - 1, yf), u),
        lerp(grad(ab, xf, yf - 1), grad(bb, xf - 1, yf - 1), u),
        v
    )
    return res  

def fractal_noise(x, y, octaves, persistence, lacunarity=2.0):
    total = 0
    amplitude = 1
    frequence = 1
    max_amplitude = 0  # Utilisé pour la normalisation

    for i in range(octaves):
        # On somme les bruits : chaque octave a une fréquence plus haute 
        # et une amplitude plus faible [5]
        total += noise(x * frequence, y * frequence) * amplitude
        
        max_amplitude += amplitude
        
        amplitude *= persistence
        frequence *= lacunarity

    # Normalisation pour rester dans l'intervalle [-1, 1] [5]
    return total / max_amplitude


def generate_map_fractale(width, height, octaves=6, persistence=0.5):
    """
    Génère une grille 2D de relief fractal (heightmap) [7].
    """
    # Initialisation de la graine et des offsets aléatoires [8, 9]
    seed_value = random.randint(0, 1000000)
    random.seed(seed_value)
    p=[]
    for i in range(1,255):
       p.append(i)
    random.shuffle(p)
    global perm
    perm = p + p+p # Table doublée pour éviter les erreurs d'index

    # Échelle de base très basse pour favoriser les grandes masses terrestres
    base_scale = random.uniform(0.005, 0.02)
    offset_x = random.uniform(-100000, 100000)
    offset_y = random.uniform(-100000, 100000)

    grid = []
    for y in range(height):
        row = []
        for x in range(width):
            # Application des décalages et de l'échelle aux coordonnées [9, 10]
            nx = (x + offset_x) * base_scale
            ny = (y + offset_y) * base_scale
            
            # Appel à la fonction de calcul fractal
            valeur = fractal_noise(nx, ny, octaves, persistence)
            row.append(valeur)
        grid.append(row)
        
    return grid


def enregistrer_monde(nom,monde):
   for x in range(longueur):
      for y in range(largeur):
         print("traitement de la tuile ",x,y)
         case=monde[y][x]
         type=case["type"]
         biome=case["biome"]
         sol=case["sol"]
         objet=case["objet"]
         gestion_db.ajoute_tuile(nom,x,y,type,biome,sol,objet,case["altitude"],case["temperature"],case["humidite"])



carte_hauteur=generer_archipel_avec_fonds(longueur,largeur,nb_iles)
carte_binaire=generer_carte_binaire(carte_hauteur)
carte_binaire=lisser_carte(carte_binaire)
carte_hauteur=combiner_hauteur_binaire(carte_hauteur,carte_binaire)
carte_temperature=generer_carte_temperature(longueur, largeur, carte_hauteur)
print(carte_temperature)
carte_hauteur,carte_riviere=appliquer_erosion_agents(carte_hauteur,carte_temperature)
carte_ressources=generer_carte_ressources(longueur, largeur)
carte_sol=generer_carte_sol(longueur, largeur)
carte_humide=generer_carte_humidite(longueur, largeur)
carte_vegetation=generer_carte_vegetation(longueur, largeur, carte_humide, carte_temperature)
monde=generer_monde_final_avec_rivieres(longueur, largeur, carte_hauteur, carte_temperature, carte_humide, carte_vegetation, carte_sol, carte_ressources, carte_riviere)
afficher_monde_couleur(monde)
#gestion_db.create_db_monde(nom)
#gestion_db.ajoute_info(nom,longueur,largeur)
#enregistrer_monde(nom,monde)
