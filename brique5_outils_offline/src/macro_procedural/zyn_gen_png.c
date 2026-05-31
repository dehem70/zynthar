/* =============================================================================
 *
 * ZYNTHAR v0.1
 *
 * Auteur : Dehem70
 * Date   : 28/05/2026
 *
 * zyn_gen_png : Convertisseur de grilles MacroChunks en fichiers images PNG (Grayscale)
 * Utilise la bibliothèque standalone stb_image_write.
 * Aligné sur l'axe horizontal longitudinal Z et adapté aux structures compressées.
 *
 * =============================================================================*/

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <zynthar.h>
#include "zyn_gen_png.h"
#include "zyn_gen_wind_global.h"


/* =============================================================================
 * EXPORTATION DE LA CARTE DE RELIEF (NIVEAU DE GRIS & MASQUE TERRE/MER)
 * ============================================================================= */

int zyn_gen_png_elevation(const MacroChunk* map, int32_t width_x, int32_t depth_z, const char* filename, const char* filename_bin) {
    if (map == NULL || width_x <= 0 || depth_z <= 0 || filename == NULL || filename_bin == NULL) return 0;

    size_t total_pixels = (size_t)width_x * (size_t)depth_z;
    
    /* Allocation des buffers d'images (1 canal de gris = 1 octet par pixel) */
    uint8_t* pixels = (uint8_t*)malloc(total_pixels * sizeof(uint8_t));
    uint8_t* pixels_bin = (uint8_t*)malloc(total_pixels * sizeof(uint8_t));
    if (pixels == NULL || pixels_bin == NULL) {
        free(pixels);
        free(pixels_bin);
        return 0;
    }

    /* Plage dynamique d'étalement pour le rendu visuel (en mètres physiques) */
    const float alt_min_rendu = ZYN_WORLD_Y_MIN;
    const float alt_max_rendu = ZYN_WORLD_Y_MAX;
    const float range_rendu = alt_max_rendu - alt_min_rendu;

    for (size_t i = 0; i < total_pixels; i++) {
        /* 1. Décompression de l'altitude du chunk (décimètres -> mètres) */
        float alt_m = DM_TO_M(map[i].elevation_max_dm);

        /* 2. Clamping visuel strict pour calibrer la dynamique du niveau de gris */
        if (alt_m < alt_min_rendu) alt_m = alt_min_rendu;
        if (alt_m > alt_max_rendu) alt_m = alt_max_rendu;

        /* 3. Normalisation linéaire entre [0.0f, 1.0f] puis passage sur 8 bits [0, 255] */
        float normalisee = (alt_m - alt_min_rendu) / range_rendu;
        pixels[i] = (uint8_t)(normalisee * 255.0f);

        /* 4. [OPTI Branchless] Génération du masque binaire terre/mer
         * On utilise le signe de l'altitude packagée : 
         * Si max_elevation > 0 (terre), la condition vaut 1, le pixel prend 255 (blanc).
         * Si max_elevation <= 0 (mer), la condition vaut 0, le pixel prend 0 (noir). */
        pixels_bin[i] = (map[i].elevation_max_dm > ZYN_SEA_LEVEL) ? 255 : 0;
    }

    /* Écriture des deux fichiers PNG via STB
       Le paramètre '1' indique un canal unique (Grayscale).
       Le dernier paramètre correspond au stride (largeur de ligne en octets, ici width_x * 1). */
    int resultat = stbi_write_png(filename, width_x, depth_z, 1, pixels, width_x);
    int resultat_bin = stbi_write_png(filename_bin, width_x, depth_z, 1, pixels_bin, width_x);
    
    /* Libération propre des buffers */
    free(pixels);
    free(pixels_bin);
    
    /* Retourne 1 uniquement si les deux fichiers ont été écrits avec succès sur le disque */
    return (resultat && resultat_bin);
}

/* =============================================================================
 * EXPORTATION DE LA CARTE DE TEMPERATURE (COPIE BRUTE ZERO-CONVERSION)
 * ============================================================================= */

int zyn_gen_png_temperature(const MacroChunk* map, int32_t width_x, int32_t depth_z, const char* filename) {
    if (map == NULL || width_x <= 0 || depth_z <= 0 || filename == NULL) return 0;
    
    size_t total_pixels = (size_t)width_x * (size_t)depth_z;
    uint8_t* pixels = (uint8_t*)malloc(total_pixels * sizeof(uint8_t));
    if (pixels == NULL) return 0;

    /* [OPTI Maximale] La température étant déjà encodée sur un octet natif uint8_t [0, 255],
     * il n'y a plus aucun calcul de conversion ni flottant à traiter. 
     * C'est un simple streaming de données direct depuis la RAM. */
    for (size_t i = 0; i < total_pixels; i++) {
        pixels[i] = map[i].temperature_raw;
    }

    int resultat = stbi_write_png(filename, width_x, depth_z, 1, pixels, width_x);
    
    free(pixels);
    return resultat;
}


/* =============================================================================
 * UTILITAIRE DE TRACÉ DE LIGNE EN RAM (ALGORITHME DE BRESENHAM)
 * ============================================================================= */
static void zyn_draw_line(uint8_t* pixels, int32_t w, int32_t h, int32_t x0, int32_t z0, int32_t x1, int32_t z1, uint8_t couleur) {
    int32_t dx = abs(x1 - x0);
    int32_t dz = abs(z1 - z0);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t sz = (z0 < z1) ? 1 : -1;
    int32_t err = dx - dz;

    while (1) {
        /* Protection stricte contre les débordements de lignes ou d'image */
        if (x0 >= 0 && x0 < w && z0 >= 0 && z0 < h) {
            pixels[z0 * w + x0] = couleur;
        }

        if (x0 == x1 && z0 == z1) break;
        int32_t e2 = 2 * err;
        if (e2 > -dz) {
            err -= dz;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            z0 += sz;
        }
    }
}

/* =============================================================================
 * EXPORTATION DU QUIVER PLOT : GRILLE DE FLÈCHES VECTORIELLES
 * ============================================================================= */
int zyn_gen_png_wind_vectors(const MacroChunk* map, int32_t width_x, int32_t depth_z, const char* filename) {
    if (map == NULL || width_x <= 0 || depth_z <= 0 || filename == NULL) return 0;

    size_t total_pixels = (size_t)width_x * (size_t)depth_z;
    uint8_t* pixels = (uint8_t*)malloc(total_pixels * sizeof(uint8_t));
    if (pixels == NULL) return 0;

    const float vitesse_max_macro = 90.0f;

    /* 1. PREMIÈRE PASSE : Génération du fond de carte standard en niveaux de gris (vitesse) */
    for (size_t i = 0; i < total_pixels; i++) {
        WindVector vent = get_global_wind(map[i].chunk_x, map[i].chunk_z);
        float vitesse = sqrtf(vent.dx * vent.dx + vent.dy * vent.dy);
        
        float normalisee = vitesse / vitesse_max_macro;
        if (normalisee > 1.0f) normalisee = 1.0f;
        if (normalisee < 0.0f) normalisee = 0.0f;

        /* On assombrit légèrement le fond de carte (multiplié par 0.5) 
         * pour que nos flèches vectorielles blanches ressortent par contraste */
        pixels[i] = (uint8_t)(normalisee * 255.0f * 0.5f);
    }

    /* 2. DEUXIÈME PASSE : Tracé de la grille de flèches sous-échantillonnée */
    const int32_t grille_pas = 16;      /* On dessine une flèche tous les 16 macro-chunks */
    const float longueur_max_fleche = 12.0f; /* Longueur maximale d'un vecteur en pixels */

    for (int32_t z = grille_pas / 2; z < depth_z; z += grille_pas) {
        for (int32_t x = grille_pas / 2; x < width_x; x += grille_pas) {
            size_t idx = (size_t)z * width_x + x;
            
            /* Échantillonnage du vecteur physique au centre de la cellule */
            WindVector vent = get_global_wind(map[idx].chunk_x, map[idx].chunk_z);
            float vitesse = sqrtf(vent.dx * vent.dx + vent.dy * vent.dy);

            if (vitesse < 1.0f) continue; /* On saute le tracé si c'est le calme plat */

            /* Calcul de la longueur de la flèche proportionnelle à la force du vent */
            float ratio_force = vitesse / vitesse_max_macro;
            if (ratio_force > 1.0f) ratio_force = 1.0f;
            float len = ratio_force * longueur_max_fleche;

            /* Direction normalisée du vecteur */
            float vx_norm = vent.dx / vitesse;
            float vz_norm = vent.dy / vitesse; // Équivalent vertical pour la carte 2D

            /* Coordonnées de départ (centre de la maille de la grille) */
            int32_t x_start = x;
            int32_t z_start = z;

            /* Coordonnées d'arrivée (pointe du vecteur de vent) */
            int32_t x_end = x + (int32_t)roundf(vx_norm * len);
            int32_t z_end = z + (int32_t)roundf(vz_norm * len);

            /* Tracé du corps du vecteur (segment blanc éclatant 255) */
            zyn_draw_line(pixels, width_x, depth_z, x_start, z_start, x_end, z_end, 255);

            /* Tracé des deux ardillons de la pointe de la flèche (calcul trigonométrique léger) */
            // Angle opposé à la direction de la flèche + ou - 45 degrés (0.707)
            float t_angle = 2.5f; /* Taille de la tête de flèche en pixels */
            
            // Rotation gauche de la pointe
            int32_t x_arrow1 = x_end - (int32_t)roundf((vx_norm * 0.707f - vz_norm * 0.707f) * t_angle);
            int32_t z_arrow1 = z_end - (int32_t)roundf((vz_norm * 0.707f + vx_norm * 0.707f) * t_angle);
            zyn_draw_line(pixels, width_x, depth_z, x_end, z_end, x_arrow1, z_arrow1, 255);

            // Rotation droite de la pointe
            int32_t x_arrow2 = x_end - (int32_t)roundf((vx_norm * 0.707f + vz_norm * 0.707f) * t_angle);
            int32_t z_arrow2 = z_end - (int32_t)roundf((vz_norm * 0.707f - vx_norm * 0.707f) * t_angle);
            zyn_draw_line(pixels, width_x, depth_z, x_end, z_end, x_arrow2, z_arrow2, 255);
        }
    }

    /* Écriture finale de l'image composites sur le disque */
    int resultat = stbi_write_png(filename, width_x, depth_z, 1, pixels, width_x);
    free(pixels);

    return resultat;
}

/* =============================================================================
 * EXPORTATION DU QUIVER PLOT DU VENT LOCAL (VECTEURS + ARRIÈRE-PLAN TOPOGRAPHIQUE)
 * ============================================================================= */
int zyn_gen_png_wind_local_vectors(const MacroChunk* map, int32_t width_x, int32_t depth_z, const char* filename) {
    if (map == NULL || width_x <= 0 || depth_z <= 0 || filename == NULL) return 0;

    // Inclusion tardive locale pour éviter les inclusions circulaires d'en-têtes
    extern WindVector zyn_gen_map_wind_local(const MacroChunk* map, int32_t width_x, int32_t depth_z, int32_t chunk_x, int32_t chunk_z);

    size_t total_pixels = (size_t)width_x * (size_t)depth_z;
    uint8_t* pixels = (uint8_t*)malloc(total_pixels * sizeof(uint8_t));
    if (pixels == NULL) return 0;

    /* Constantes de rendu de l'univers physique */
    const float alt_min_rendu = ZYN_WORLD_Y_MIN;
    const float range_rendu = (float)ZYN_WORLD_Y_MAX - alt_min_rendu;
    const float vitesse_max_local = 140.0f; /* Plafond configuré du vent local */

    /* 1. PREMIÈRE PASSE : Génération du relief amorti en arrière-plan */
    for (size_t i = 0; i < total_pixels; i++) {
        float alt_m = DM_TO_M(map[i].elevation_max_dm);
        if (alt_m < alt_min_rendu) alt_m = alt_min_rendu;
        if (alt_m > (float)ZYN_WORLD_Y_MAX) alt_m = (float)ZYN_WORLD_Y_MAX;

        float normalisee = (alt_m - alt_min_rendu) / range_rendu;
        
        /* Amortissement du fond (multiplié par 0.4) pour que les flèches ressortent parfaitement */
        pixels[i] = (uint8_t)(normalisee * 255.0f * 0.4f);
    }

    /* 2. DEUXIÈME PASSE : Échantillonnage et tracé de la grille de flèches altérées */
    const int32_t grille_pas = 16;           /* Densité de la grille */
    const float longueur_max_fleche = 12.0f; /* Longueur maximale en pixels */

    for (int32_t z = grille_pas / 2; z < depth_z; z += grille_pas) {
        for (int32_t x = grille_pas / 2; x < width_x; x += grille_pas) {
            size_t idx = (size_t)z * width_x + x;
            
            /* Échantillonnage du vent LOCAL (Topographique) */
            WindVector vent = zyn_gen_map_wind_local(map, width_x, depth_z, map[idx].chunk_x, map[idx].chunk_z);
            float vitesse = sqrtf(vent.dx * vent.dx + vent.dy * vent.dy);

            if (vitesse < 1.0f) continue; /* Calme plat, pas de flèche */

            /* Calcul de la longueur proportionnelle à la force locale */
            float ratio_force = vitesse / vitesse_max_local;
            if (ratio_force > 1.0f) ratio_force = 1.0f;
            float len = ratio_force * longueur_max_fleche;

            float vx_norm = vent.dx / vitesse;
            float vz_norm = vent.dy / vitesse;

            int32_t x_start = x;
            int32_t z_start = z;
            int32_t x_end = x + (int32_t)roundf(vx_norm * len);
            int32_t z_end = z + (int32_t)roundf(vz_norm * len);

            /* Tracé du segment de flux principal (Blanc pur) */
            zyn_draw_line(pixels, width_x, depth_z, x_start, z_start, x_end, z_end, 255);

            /* Tracé de la tête de flèche directionnelle */
            float t_angle = 2.5f;
            int32_t x_arrow1 = x_end - (int32_t)roundf((vx_norm * 0.707f - vz_norm * 0.707f) * t_angle);
            int32_t z_arrow1 = z_end - (int32_t)roundf((vz_norm * 0.707f + vx_norm * 0.707f) * t_angle);
            zyn_draw_line(pixels, width_x, depth_z, x_end, z_end, x_arrow1, z_arrow1, 255);

            int32_t x_arrow2 = x_end - (int32_t)roundf((vx_norm * 0.707f + vz_norm * 0.707f) * t_angle);
            int32_t z_arrow2 = z_end - (int32_t)roundf((vz_norm * 0.707f - vx_norm * 0.707f) * t_angle);
            zyn_draw_line(pixels, width_x, depth_z, x_end, z_end, x_arrow2, z_arrow2, 255);
        }
    }

    int resultat = stbi_write_png(filename, width_x, depth_z, 1, pixels, width_x);
    free(pixels);
    return resultat;
}

/* =============================================================================
 * EXPORTATION DE LA CARTE D'HUMIDITÉ TEMPORAIRE (STREAME BRUT DEPUIS LE BUFFER)
 * ============================================================================= */
int zyn_gen_png_humidity(const MacroChunk* map, int32_t width_x, int32_t depth_z, const char* filename) {
    if (map == NULL || width_x <= 0 || depth_z <= 0 || filename == NULL) return 0;
    
    size_t total_pixels = (size_t)width_x * (size_t)depth_z;
    uint8_t* pixels = (uint8_t*)malloc(total_pixels * sizeof(uint8_t));
    if (pixels == NULL) return 0;

    /* On lit directement l'octet brut temporaire stocké dans map[i].biome */
    for (size_t i = 0; i < total_pixels; i++) {
        pixels[i] = map[i].biome;
    }

    int resultat = stbi_write_png(filename, width_x, depth_z, 1, pixels, width_x);
    free(pixels);
    return resultat;
}
