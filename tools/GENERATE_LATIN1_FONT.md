# Génération d'une police Latin-1 pour Adafruit GFX (ESP32)

Ce guide produit `DejaVuSans9ptLat1.h` couvrant 0x20–0xFF (accents français) utilisable avec votre option fontIndex=4.

## 1. Préparer l'outil fontconvert

Adafruit fournit `fontconvert` dans le dépôt Adafruit-GFX-Library.

```bash
# Cloner (si pas déjà) dans un dossier temporaire
git clone https://github.com/adafruit/Adafruit-GFX-Library.git /tmp/Adafruit-GFX-Library
cd /tmp/Adafruit-GFX-Library/fontconvert
make
# Produit l'exécutable ./fontconvert
```

Sur macOS il faut parfois : `brew install freetype`

## 2. Localiser le fichier TTF

DejaVu Sans est souvent installé. Sinon téléchargez : https://dejavu-fonts.github.io/

Supposons le chemin : `~/Library/Fonts/DejaVuSans.ttf`

## 3. Générer la plage étendue

La syntaxe : `fontconvert <ttf> <taille_point> <first> <last>`

```bash
cd /tmp/Adafruit-GFX-Library/fontconvert
./fontconvert ~/Library/Fonts/DejaVuSans.ttf 9 32 255 > DejaVuSans9ptLat1.h
```

## 4. Adapter le nom interne

Le fichier généré contiendra une structure `GFXfont DejaVuSans9pt7b`. Renommez-la:

- Nom bitmap/glyph pas critique, mais renommer l'objet final en `DejaVuSans9ptLat1`.
- Mettre gardes d'inclusion et `#pragma once` éventuel.

Exemple modifications rapides :
```bash
sed -i '' 's/DejaVuSans9pt7b/DejaVuSans9ptLat1/g' DejaVuSans9ptLat1.h
```

## 5. Copier dans le projet

```bash
cp DejaVuSans9ptLat1.h \
  "/Users/electron_rare/Downloads/ESP32 P10 RGB 32x16 Digital Clock/examples/DejaVuSans9ptLat1.h"
```

(Écrase le placeholder.)

## 6. Vérifier la taille mémoire

Ouvrir le header et vérifier la ligne finale (first=0x20, last=0xFF). La flash augmente (quelques Ko). Sur votre build (~62% flash), marge OK.

## 7. Recompiler l'environnement web

```bash
pio run -e fullscreen_countdown_web
pio run -e fullscreen_countdown_web --target upload
```

## 8. Test

Dans l'UI, choisir la police "Latin-1 (Accents)" et saisir un titre: `Événement Noël à l'école – Déjà fini ?` pour vérifier É, à, é, ù, ô, ç.

## 9. Option: Réduire la plage (optimisation)

Si vous voulez seulement : chiffres, majuscules accentuées, minuscules accentuées de base.

Créer un fichier texte listant les caractères requis, puis utiliser un outil comme `gfxfontgen` (projets tiers) permettant liste personnalisée. Sinon conserver plage complète pour simplicité.

## 10. Rollback

Si mémoire insuffisante, supprimez `-DHAVE_LATIN1_FONT` dans `platformio.ini` et/ou revenez au placeholder.

---
Astuce: Garder le header généré sous contrôle de version pour éviter régénération.

Fin.
