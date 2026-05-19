import sqlite3

def create_db_monde(nom):
   cnx=sqlite3.connect(nom+".db")
   c=cnx.cursor()
   req="CREATE TABLE IF NOT EXISTS tuiles (num INTEGER PRIMARY KEY, x INTEGER,y INTEGER, type INTEGER, biome INTEGER, sol INTEGER, objet INTEGER)"
   c.execute(req)
   req="CREATE TABLE IF NOT EXISTS info (nom TEXT PRIMARY KEY, xmax INTEGER, ymax INTEGER)"
   c.execute(req)
   req="CREATE TABLE IF NOT EXISTS type (num INTEGER PRIMARY KEY AUTOINCREMENT, type TEXT)"
   c.execute(req)
   req="CREATE TABLE IF NOT EXISTS biome (num INTEGER PRIMARY KEY AUTOINCREMENT, biome TEXT)"
   c.execute(req)
   req="CREATE TABLE IF NOT EXISTS sol (num INTEGER PRIMARY KEY AUTOINCREMENT, sol TEXT)"
   c.execute(req)
   req="CREATE TABLE IF NOT EXISTS objet (num INTEGER PRIMARY KEY AUTOINCREMENT, objet TEXT)"
   c.execute(req)
   cnx.commit()
   cnx.close()
   return True

def ajoute_liste_caracteristique(nom,caract,liste):
   for l in liste:
      ajoute_caracteristique(caract,l,nom)
   return True

def ajoute_caracteristique(nom,caract,valeur):
   cnx=sqlite3.connect(nom+".db")
   c=cnx.cursor()
   try:
      req='INSERT INTO '+caract+' VALUES (NULL,"'+valeur+'")'
      c.execute(req)
      cnx.commit()
   except:
      print("pb dans insert de ",valeur," dans table ",caract)
   cnx.close()
   return True

def lire_id_caracteristique(nom,caract,valeur):
   cnx=sqlite3.connect(nom+".db")
   c=cnx.cursor()
   r=[]
   result=-1
   try:
      req='SELECT num FROM '+caract+' WHERE '+caract+'="'+valeur+'"'
      print(req)
      c.execute(req)
      r=c.fetchall()
   except:
      print("pb dans SELECT de ",valeur," dans table ",caract)
   cnx.close()
   if len(r)>0:
      result=r[0][0]
   return result

def ajoute_info(nom,xmax,ymax):
   cnx=sqlite3.connect(nom+".db")
   c=cnx.cursor()
   try:
      req='INSERT INTO info VALUES ("'+nom+'",'+str(xmax)+','+str(ymax)+')'
      c.execute(req)
      cnx.commit()
   except:
      print("pb dans insert info ",nom," avec valeur ",xmax,ymax)
   cnx.close()
   return True

def ajoute_tuile(nom,x,y,typ,biome,sol,objet):
   cnx=sqlite3.connect(nom+".db")
   c=cnx.cursor()
   list_type=[]
   list_biome=[]
   list_sol=[]
   list_objet=[]
   try:
      req='SELECT xmax,ymax FROM info WHERE nom="'+nom+'"'
      c.execute(req)
      info=c.fetchall()
      xmax=info[0][0]
      ymax=info[0][1]
   except:
      print("pb dans SELECT info ",nom, info)
   num_tuile=xmax*y+x
   try:
      id_type=list_type.index(typ)
   except:
      ajoute_caracteristique(nom,"type",typ)
      list_type.append(typ)
      id_type=list_type.index(typ)
      
   try:
      id_objet=list_objet.index(objet)
   except:
      ajoute_caracteristique(nom,"objet",objet)
      list_objet.append(objet)
      id_objet=list_objet.index(objet)

   try:
      id_biome=list_biome.index(biome)
   except:
      ajoute_caracteristique(nom,"biome",biome)
      list_biome.append(biome)
      id_biome=list_biome.index(biome)

   try:
      id_sol=list_sol.index(sol)
   except:
      ajoute_caracteristique(nom,"sol",sol)
      list_sol.append(sol)
      id_sol=list_sol.index(sol)

   try:
      req='INSERT INTO tuiles VALUES ('+str(num_tuile)+','+str(x)+','+str(y)+','+str(id_type)+','+str(id_biome)+','+str(id_sol)+','+str(id_objet)+')'
      c.execute(req)
      cnx.commit()
   except:
      print("pb dans insert tuile ",nom," avec valeurs ",x,y)
   cnx.close()
   return num_tuile
