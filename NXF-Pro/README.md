# NXF Pro

Gestionnaire de fichiers natif pour Nintendo Switch, avec un **véritable serveur FTP**
intégré : pilotez la carte SD et la mémoire interne de la console depuis un PC,
un Mac, un téléphone ou un autre appareil du même réseau, supprimez
complètement les jeux installés, et retrouvez un menu **DBI** pour le FTP et les
fichiers NSP.

Interface noire premium, typographie Montserrat, dégradés animés et animations
fluides — sans jamais sacrifier la réactivité sur la console.

![Explorateur, vue liste](docs/explorateur-liste.png)

## Sommaire

- [Fonctionnalités](#fonctionnalités)
- [Applications installées](#applications-installées)
- [Première configuration](#première-configuration)
- [DBI](#dbi)
- [Installation](#installation)
- [Serveur FTP](#serveur-ftp)
- [Raccourcis](#raccourcis)
- [Compilation](#compilation)
- [Configuration](#configuration)
- [Limites connues](#limites-connues)
- [Licences](#licences)

## Fonctionnalités

### Explorateur réel

Toutes les opérations passent par de vraies appels système (`opendir`, `stat`,
`mkdir`, `rename`, `unlink`, `rmdir`) : rien n'est simulé.

- Deux stockages : **carte SD** (`sdmc:`) et **mémoire interne** (partition NAND
  utilisateur, quand elle est accessible).
- Navigation par dossiers, retour, fil d'Ariane cliquable.
- Créer un dossier, créer un fichier texte, renommer fichiers **et** dossiers.
- Copier, couper, coller, déplacer — y compris des dossiers entiers, avec
  résolution automatique des noms en conflit (`nom (1).txt`).
- Supprimer avec confirmation explicite.
- Sélection multiple, propriétés détaillées (nom, type, taille, date, chemin).
- Tri par nom, taille ou date, croissant ou décroissant.
- Recherche instantanée insensible à la casse.
- Vue liste et vue grille, icônes distinctes par format (dossier, image, audio,
  vidéo, archive, code, texte).
- Espace libre et capacité totale affichés en en-tête.

### Interface

- Fond noir pur `#000000`, barre latérale gris foncé, aucun fond clair.
- Barre latérale fixe : logo NXF Pro, sections STOCKAGE / RÉSEAU / APPLICATION.
- Titre « NXF Pro » en haut à gauche de la zone principale, en Montserrat Light,
  avec un dégradé arc-en-ciel qui défile en boucle sur les lettres.
- Accents dégradés, coins arrondis, ombres discrètes, transitions à chaque état.
- Animations : apparition échelonnée des lignes, curseur glissant, barre
  colorée de la section active, indicateur de navigation animé, panneaux qui
  apparaissent en fondu + échelle, notifications en fondu.
- Touchscreen : sections de la barre latérale, lignes, cartes et bouton FTP.

![Explorateur, vue grille](docs/explorateur-grille.png)

### Typographie

Montserrat est la seule police de l'application, embarquée dans le binaire :

| Graisse | Usage |
| --- | --- |
| Light | textes secondaires, chemins, métadonnées |
| Regular | noms de fichiers, boutons, contenus |
| Medium | titres de sections, en-têtes, accents |

Aucune police système n'est utilisée. Les fichiers `.ttf` et leur licence
(`assets/fonts/OFL.txt`, SIL Open Font License 1.1) sont fournis.

## Applications installées

La section **Applications** (barre latérale, groupe CONSOLE) liste tous les
titres installés sur la console : jeux, homebrews et applications système.
Chaque ligne affiche l'icône officielle, le nom, l'éditeur, l'identifiant du
titre, la version et la présence de mises à jour ou de DLC.

![Applications installées](docs/applications.png)

`A` (ou `Y`) ouvre la suppression **complète** du titre sélectionné. Après
confirmation, NXF Pro efface :

1. le jeu lui-même, sur la mémoire interne comme sur la carte SD ;
2. toutes ses mises à jour ;
3. tous ses DLC ;
4. ses fichiers de sauvegarde, sur la NAND et sur la carte SD ;
5. les dossiers de contenu Atmosphère liés au titre (`atmosphere/contents/<id>`
   et `atmosphere/titles/<id>`), donc mods et cheats.

Il ne reste plus aucune trace du jeu après l'opération, et la liste se met à
jour immédiatement.

![Confirmation de suppression](docs/applications-suppression.png)

> La suppression utilise le service `ns` de la console : elle exige un lancement
> en **mode application** (titre complet), comme toutes les opérations sur les
> titres installés.

## Première configuration

Au tout premier lancement, NXF Pro affiche un écran de configuration : le logo,
le titre « NXF Pro » en dégradé animé, le texte *Choose your language :* et une
liste défilante de douze langues (English, Français, Español, Deutsch, Italiano,
Português, Nederlands, Polski, Russian, Japanese, Korean, Chinese).

![Première configuration](docs/premiere-configuration.png)

Le bouton **Enregistrer** crée le dossier `sdmc:/NXF_Save` et y écrit
`pref.json` :

```json
{
  "language": "fr",
  "ftpPort": 5000,
  "ftpUser": "switch",
  "ftpPassword": "nxfpro",
  "animations": true,
  "grid": false
}
```

À chaque lancement, le fichier est lu : s'il existe, l'application démarre
directement dans la langue enregistrée ; sinon l'écran de configuration
réapparaît. L'interface est traduite en **français et en anglais** ; les autres
langues sont conservées dans `pref.json` et utilisent l'anglais en attendant
leurs traductions.

## DBI

Le menu **DBI** regroupe les outils de transfert et d'installation dans quatre
cartes :

![DBI](docs/dbi.png)

- **Démarrer le serveur FTP** — démarre le serveur intégré sans quitter la page
  (adresse, port et point vert d'activité affichés).
- **Installer un NSP** — liste les fichiers `.nsp` trouvés sur la carte SD
  (`/`, `/switch`, `/NXF_Save`, `/nsp`, `/roms`), avec leur taille, leur
  identifiant de titre et un badge **Installé** quand le jeu est déjà présent.
- **Contenu installé** — accès direct à la liste des applications.
- **Stockage** — espace libre et capacité totale de la carte SD.

![Fichiers NSP](docs/dbi-nsp.png)

Quand un `.nsp` correspond à un jeu déjà installé, NXF Pro demande
**« Supprimer le contenu de cette app ? »** en affichant l'icône et le nom du
jeu, avec deux boutons **Oui** et **Non**. « Non » annule et affiche
*Installation annulé car contenu non supprimé.*

![Conflit NSP](docs/nsp-conflit.png)

> **Important** : cette version ne contient pas le moteur d'écriture NCM/NSP
> (analyse du CNMT et écriture des NCA), qui exige les clés de la console.
> L'écriture d'un NSP n'est donc pas encore possible : NXF Pro refuse
> l'opération **sans jamais supprimer le contenu existant**, pour ne pas laisser
> un jeu effacé sans remplaçant. La détection, le dialogue et la suppression de
> contenu, eux, sont réels.

## Installation

1. Téléchargez `NXF-Pro.nro` (dernière version du dépôt ou release).
2. Copiez-le sur la carte SD dans `switch/`.
3. Lancez-le depuis le menu homebrew (Atmosphère).

Les préférences sont créées automatiquement dans `sdmc:/NXF_Save/pref.json`.

> Aucun NSP n'est fourni : sa génération exige les clés privées de la console.

## Serveur FTP

Le serveur est une implémentation FTP complète (sous-ensemble RFC 959 utilisé par
les clients courants) qui tourne sur son propre thread, en mode passif (PASV).

![Serveur FTP](docs/serveur-ftp.png)

1. Ouvrez la section **FTP** et appuyez sur **Démarrer**.
2. L'adresse s'affiche, par exemple `ftp://192.168.1.42:5000`.
3. Connectez-vous depuis un autre appareil :

```bash
# curl
curl ftp://switch:nxfpro@192.168.1.42:5000/sd/

# FileZilla / Finder / Explorateur Windows : hôte 192.168.1.42, port 5000
```

Identifiants par défaut : utilisateur `switch`, mot de passe `nxfpro`.
Le port, l'utilisateur et le mot de passe se changent dans **Paramètres**, et
sont conservés sur la carte SD.

Arborescence exposée :

| Chemin FTP | Contenu |
| --- | --- |
| `/sd` | carte SD (`sdmc:`) |
| `/internal` | mémoire interne (NAND utilisateur), si montée |

Commandes prises en charge : `USER`, `PASS`, `SYST`, `FEAT`, `OPTS`, `TYPE`,
`PWD`, `CWD`, `CDUP`, `PASV`, `LIST`, `NLST`, `RETR`, `STOR`, `APPE`, `SIZE`,
`MDTM`, `MKD`, `RMD`, `DELE`, `RNFR`, `RNTO`, `NOOP`, `QUIT`.
Les noms UTF-8 sont transmis tels quels, et les chemins sont normalisés : aucune
évasion hors des deux racines n'est possible.

Le panneau **Activité** journalise les connexions et les transferts en direct.

## Raccourcis

| Touche | Action |
| --- | --- |
| `A` | Ouvrir / valider |
| `B` | Retour / fermer |
| `X` | Sélectionner |
| `Y` | Menu d'actions |
| `L` | Vue liste / grille |
| `R` | Changer le tri |
| `−` | Rechercher |
| `ZL` / `ZR` | Section précédente / suivante |
| `+` | Quitter |
| Stick / croix | Naviguer |

Dans **Applications**, `A` ou `Y` ouvre la suppression définitive du titre
sélectionné.

## Compilation

Prérequis : [devkitPro](https://devkitpro.org/) avec `devkitA64` et `libnx`.

```bash
export DEVKITPRO=/opt/devkitpro
cd NXF-Pro
make -j$(nproc)
```

Produit `NXF-Pro.nro`, `NXF-Pro.elf` et `NXF-Pro.nacp`.

### Vérifications hors console

L'interface et le serveur FTP sont testables sans Switch :

```bash
# 1. Rendu de tous les écrans en images (aucune dépendance libnx)
g++ -std=c++17 -O2 -Iinclude -Ivendor \
    src/canvas.cpp src/font.cpp src/icons.cpp src/model.cpp src/render.cpp \
    src/app.cpp tools/preview_ui.cpp -o nxfpreview
./nxfpreview assets build/preview

# 2. Protocole FTP réel, piloté par curl
g++ -std=c++17 -O2 -Iinclude -Ivendor -include tools/host/switch_shim.h \
    src/ftp_server.cpp src/model.cpp tools/host/ftp_test.cpp -o ftptest -lpthread
./ftptest
```

Le second test démarre le vrai serveur et vérifie avec `curl` : listing, RETR,
STOR, MKD, RMD, DELE, renommage, authentification et confinement des chemins.

## Configuration

`sdmc:/NXF_Save/pref.json`

```json
{
  "language": "fr",
  "ftpPort": 5000,
  "ftpUser": "switch",
  "ftpPassword": "nxfpro",
  "animations": true,
  "grid": false
}
```

## Limites connues

- La mémoire interne (NAND utilisateur) n'est montée qu'en **mode application**
  (lancement en titre complet). En mode applet, la section l'indique clairement
  au lieu d'afficher un faux contenu.
- Un seul client FTP à la fois : suffisant pour un usage familial, plus simple à
  rendre robuste sur console.
- Aucun NSP : la génération nécessite les clés de la console.

## Licences

- Code NXF Pro : MIT (voir `LICENSE` à la racine du dépôt).
- Montserrat : SIL Open Font License 1.1 (`assets/fonts/OFL.txt`).
- `stb_truetype` / `stb_image` : domaine public (Sean Barrett).
- `libnx` : ISC (devkitPro).
