La brique 2 sera conçue pour être autonome.

Un front end communiquera avec les autres briques par la mémoire partagée

Le front ent communiquera avec une chaîne de production de workers par le réseau (TCP Brut Asynchrone (avec FlatBuffers))

Le font end communique avec le worker0 pour lui envoyer les données nécessaire à la génération du micro chunk (id macrochunk, coordonnées X/Y/Z du micro chunck dans le macro chunk et LOD)

Dans la chaîne, un micro chunk est une structure de 256x256x256 voxels (16777216 voxels au total). Chaque voxel sera codé par sa matiere en uint8_t (64 matieres envisagées) - dans un second temps possible de compresser les voxels par 4 pour comprimer les structures dans le cache (et avoir plus de micro chunk dans le cache).

La chaîne sera composé de 8 workers + 1 surveillant indépendant communiquant via la mémoire partagé.

Au lancement de la chaîne, les fichiers sqlite3 de la base de données sont chargées dans un ramdisk (création du ramdisk au lancement par le surveillant, avant copie des fichiers sqlite3)

# Composition de la chaîne et rôle de chacun
Les workers communiqueront entre eux via la mémoire partagé. En mémoire partagé il y aura 8 espaces de travail (16777216 octets + 16 octets pour meta données) pour la construction des macro chunk. Un table de 8 octets indiquera l'état des espaces de travail (0 dispo, et ensuite le numéro du workers qui doit travailler dessus)

## worker0  (récupération métadonnée):
Entrée : 
	via le reseau (TCP brut asynchrone), réception d'une structure contenant  :
			- Id macro chunk (uint32_t)
			- coordonnées relatives du micro chunk dans le macro chunk (X/Z/Y)
				- Les limites sont à recalculer en utilisant les limites du monde dans zynthar.h. si les limites dépassent 255 => erreurs dimensions monde incohérentes
				- X (uint_8_t) compris entre 0 et 19  inclus
				- Z (uint_8_t) compris entre 0 et 19 inclus
				- Y (uint_8_t) compris entre 0 et  119 inclus
			- niveau de LOD (uint_8) compris entre 0 et 2 inclus
			- seed du monde (uint32_t)
			- numéro de job (uint32_t) - 31 bits pour codage numéro de job, 1 bit pour la validité (si à 0, job non valide, le worker ne travaille pas dessus et le passe directement au suivant)
Sortie :
	un espace de travail initialisé
Fonction :
	1 - recherche d'un espace de travail libre (état à 0)
	2 - vérifier validité des datas demandées :
			si ok on continue
			si nok, job non valide (renseignement dans numéro de job)
				 et saut vers étape 6
	3 - récupération dans la base de données relief des macro données sur macro chunk (et de la seed dans la table spécial) -> si le job correspond déjà a un job en cours, gestion à prévoir
	4 - enregistrement des méta données dans espace de travail
	5 - vérification du cache :
		5.1 - si présent dans le cache, copie du cache dans l'espace de travail et effacement du cache (juste changement d'état d'un bits pour dire que la place est libre)
		5.2 - si pas présent init à 0 de toutes les cases de l'espace de travail (0 = air)
	6- si job valide , 
		6.1 - si microchunk déja dans le cache à l'état 4, on le copie dans l'espace de travail (en LOD0) et on passe l'état 5 pour worker décorateur
		6.2 - si pas dans le cache, on passe l'état à 1 pour calcul dans la chaine
	7- si job invalide on passe l'état à 7 (le dernier worker pourra répondre job invalide au serveur)

## worker1 (construction micro relief):
Entrée :
	espace de travail à l'état 1
Sortie :
	espace de travail avec surface du micro chunk calculée d'apres les macro données
Fonction :
	1 - détermination du micro relief d'apres les meda données et la seed du monde de façon déterministe en LOD0
	2 - enregistrement du micro relief dans l'espace de travail (en fonction du biome dans le macro chunk)
	3 - passage de l'état à 2

## worker2 (ajout des rivieres):
Entrée :
	espace de travail à l'état 2
Sortie :
	espace de travail avec rivieres ajoutées
Fonction :
	1 - interrogation de la base de données rivieres
	2 - détermination des rivieres si besoin d'apres les meda données et la seed du monde de façon déterministe en LOD0
	3 - enregistrement des rivieres dans l'espace de travail
	4 - passage de l'état à 3
## worker3 (ajout des grottes):
Entrée :
	espace de travail à l'état 3
Sortie :
	espace de travail avec grottes ajoutées
Fonction :
	1 - interrogation de la base de données grottes
	2 - détermination des grottes si besoin d'apres les meda données et la seed du monde de façon déterministe en LOD0
	3 - enregistrement des grottes dans l'espace de travail
	4 - passage de l'état à 4

## worker4 (ajout des ressources):
Entrée :
	espace de travail à l'état 4
Sortie :
	espace de travail avec ressources ajoutées
Fonction :
	1 - interrogation de la base de données ressources
	2 - détermination des ressources si besoin d'apres les meda données et la seed du monde de façon déterministe en LOD0
	3 - enregistrement des ressources dans l'espace de travail
	4 - passage de l'état à 5
## worker5 (décorateur):
Entrée :
	espace de travail à l'état 5
Sortie :
	espace de travail avec décorations ajoutées
Fonction :
	1 - interrogation de la base de données décoration (les objets sont dans une base séparée de la base delta)
	2 - détermination des décorations si besoin en LOD0
	3 - enregistrement des décorations dans l'espace de travail
	4 - passage de l'état à 6

## worker6 (delta):
Entrée :
	espace de travail à l'état 6
Sortie :
	espace de travail avec deltas appliqués
Fonction :
	1 - interrogation de la base de données deltas
	2 - détermination des deltas si besoin en LOD0
	3 - enregistrement des deltas dans l'espace de travail
	4 - passage de l'état à 7

## worker7 ( LOD + compression):
Entrée :
	espace de travail à l'état 7
Sortie :
	espace de travail libérée
	réponse au serveur via le reseau (TCP brut asynchrone) en envoyant le micro chunk de façon compressé RLE
Fonction :
	1 - enregistrement dans le cache (au sommet de la pile)
	2 - adaptation du microchunk à la LOD demandé
	3 - compresion RLE
	4 - réponse au serveur
	5- espace de travail repasse à l'état 0