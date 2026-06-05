/* =============================================================================
 *
 * ZYNTHAR v.0.1.0
 *
 * Auteur : Dehem70
 * Date   : 03/06/2026
 *
 * zyn_utils  :
 * utilisation :
 *
 * =============================================================================*/
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

// En-têtes requis sous Linux/POSIX pour manipuler 'struct stat' et 'mkdir'
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <zynthar.h>


void normaliser(float* tableau, float min,float max,size_t total_cases,float vmin,float vmax) {
    const float inv_max = vmax / max;
    const float inv_min = vmin / min; 
 
    for (int32_t p = 0; p < total_cases; p++) {
        float val = tableau[p];
        float cond_le = (float)(val <= 0.0f);
        float cond_gt = (float)(val > 0.0f);
        tableau[p] = val * (cond_le * inv_min + cond_gt * inv_max);
    }

}
