import requests
from PIL import Image
import io
import sys

# --- CONFIGURATION DE LA ZONE À TESTER ---
# On va tester une grille de chunks de (X_START à X_END) et (Y_START à Y_END)
X_START, X_END = 1, 18  # Chunks X de 3 à 7
Y_START, Y_END = 1, 18  # Chunks Y de 3 à 7
API_URL = "http://127.0.0.1:8000/api/world"

def download_chunk_image(x, y):
    url = f"{API_URL}/chunk/{x}/{y}/heightmap.png"
    try:
        response = requests.get(url, timeout=5)
        if response.status_code != 200:
            return None
        return Image.open(io.BytesIO(response.content)).convert("L").load()
    except Exception:
        return None

print("\n" + "="*70)
print(f"🛰️  LANCEMENT DU SCANNER DE COUTURES GLOBAL (Zone: Chunks {X_START} à {X_END})")
print("="*70)

# Pré-chargement des pixels en mémoire pour éviter de surcharger le serveur de requêtes répétitives
print("📥 Téléchargement des images de la grille en cours...")
grid_pixels = {}
for x in range(X_START, X_END + 1):
    for y in range(Y_START, Y_END + 1):
        pixels = download_chunk_image(x, y)
        if pixels:
            grid_pixels[(x, y)] = pixels
        else:
            print(f"❌ Impossible de charger le chunk ({x},{y}). Vérifie que server.py tourne.")
            sys.exit(1)

print(f"✅ {len(grid_pixels)} chunks chargés avec succès. Début de l'analyse mathématique...\n")

erreurs_est_ouest = 0
erreurs_nord_sud = 0
tests_corrects = 0

# --- 1. VÉRIFICATION DES COUTURES EST-OUEST (Verticales) ---
for y in range(Y_START, Y_END + 1):
    for x in range(X_START, X_END):
        pix_gauche = grid_pixels[(x, y)]
        pix_droit = grid_pixels[(x + 1, y)]
        
        # On compare la colonne 100 du chunk de gauche avec la colonne 0 du chunk de droite
        frontiere_valide = True
        for row in range(101):
            if pix_gauche[100, row] != pix_droit[0, row]:
                frontiere_valide = False
                erreurs_est_ouest += 1
                
        if frontiere_valide:
            tests_corrects += 1
        else:
            print(f"⚠️  DÉFAUT COUTURE EST-OUEST détecté entre le chunk ({x},{y}) et ({x+1},{y})")

# --- 2. VÉRIFICATION DES COUTURES NORD-SUD (Horizontales) ---
for x in range(X_START, X_END + 1):
    for y in range(Y_START, Y_END):
        pix_haut = grid_pixels[(x, y + 1)]
        pix_bas = grid_pixels[(x, y)]
        
        # Avec notre correctif (100 - row) :
        # Le bas du chunk du HAUT (row=100) doit toucher le haut du chunk du BAS (row=0)
        frontiere_valide = True
        for col in range(101):
            if pix_haut[col, 100] != pix_bas[col, 0]:
                frontiere_valide = False
                erreurs_nord_sud += 1
                
        if frontiere_valide:
            tests_corrects += 1
        else:
            print(f"⚠️  DÉFAUT COUTURE NORD-SUD détecté entre le chunk ({x},{y+1}) (Haut) et ({x},{y}) (Bas)")

# --- BILAN FINAL ---
print("\n" + "="*70)
print("📊 RAPPORT DE SÉCURITÉ GÉOMÉTRIQUE")
print("="*70)
total_erreurs = erreurs_est_ouest + erreurs_nord_sud

if total_erreurs == 0:
    print(f"🟩 RÉSULTAT : 100% PARFAIT ! ({tests_corrects} frontières analysées et validées)")
    print("   Le monde est mathématiquement hermétique. Aucune faille possible en jeu.")
else:
    print(f"🟥 RÉSULTAT : ÉCHEC DE VALIDATION DU MONDE")
    print(f"   - Écarts détectés sur l'axe Est-Ouest (X) : {erreurs_est_ouest} pixels asymétriques")
    print(f"   - Écarts détectés sur l'axe Nord-Sud (Y)  : {erreurs_nord_sud} pixels asymétriques")
    print("\n💡 Conseil : Si tu as des erreurs, revérifie la gestion de (100 - row) ou l'inversion")
    print("   col/row dans la route PNG de ton 'server.py'.")
print("="*70 + "\n")
