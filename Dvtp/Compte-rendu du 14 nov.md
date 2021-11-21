# Compte-rendu de la réunion du 03 novembre 2021

Présents : tous le groupe

## Ordre du jour
Fonctionnalité implémentée dans create_task()
Les fonctions auxiliaires :
-    isBigEndian() vérifie si la conversion est nécessaire (pour éviter le cas bigEndian -> littleEndian -> envoi à saturnd)
-    create_path(...) – pour obtenir les paths des stacks à partir de path par default ou celui qui est passé comme option
     Particularité d’utilisation de goto : toute les variables doivent être déclaré au début de fonction (avant de premier goto) , sinon on aura les effets indésirables/imprévisibles
     Macros pour les free() et close()
     Marcos pour taille de struct timing (eviter à utiliser sizeof(struct timing)) et pour taille de opcode

## TODO
-[ ] attribution de nouvelles issues

Prochaine réunion : samedi 13 novembre après-midi