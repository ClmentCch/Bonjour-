# Polices

## `LiberationSerif-Regular.ttf` (présente dans le dépôt)

Substitut **légal et redistribuable** de Times New Roman : Liberation Serif est dessiné
avec des métriques compatibles (mêmes largeurs, mêmes proportions), sous
**SIL Open Font License 1.1** (`LICENSE-Liberation.txt`).

C'est cette police qui est embarquée dans le `Bonjour.nro` publié, car Times New Roman
ne peut pas être redistribuée.

Source : <https://github.com/liberationfonts/liberation-fonts> (version 2.1.5).

## `times.ttf` (non présente, fournie par l'utilisateur)

Times New Roman Regular est une police **propriétaire (Monotype)**. Elle n'est **jamais**
committée dans ce dépôt (voir `.gitignore`).

Deux façons de l'utiliser :

1. **Sans recompiler** — copier votre fichier sur la carte SD :

   ```
   SD:/switch/Bonjour/times.ttf
   ```

   L'application le charge au démarrage à la place de la police embarquée.

2. **À la compilation** — placer le fichier ici sous le nom exact `times.ttf` :

   ```
   assets/fonts/times.ttf
   ```

   Le `Makefile` le détecte (`-DBONJOUR_FONT_IS_TIMES=1`) et l'embarque dans le NRO.
   `make clean && make` pour forcer la régénération.

Ordre de priorité au démarrage :

1. `sdmc:/switch/Bonjour/times.ttf` (carte SD)
2. police embarquée à la compilation (`times.ttf` si fournie, sinon Liberation Serif)
