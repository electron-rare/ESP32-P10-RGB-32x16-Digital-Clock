#!/usr/bin/env bash
set -euo pipefail

# Script de génération d'une police Latin-1 (0x20-0xFF) pour Adafruit GFX
# Produit: DejaVuSans9ptLat1.h (remplace le placeholder existant)
# Plateformes: macOS / Linux
# Dépendances: git, make, g++, freetype (brew install freetype), curl (si téléchargement du TTF)

POINT_SIZE=9
FIRST_CODE=32
LAST_CODE=255
FONT_TTF=""
FONT_NAME="DejaVuSans"
OUTPUT_NAME="DejaVuSans9ptLat1.h"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXAMPLES_DIR="${PROJECT_ROOT}/examples"
TOOLS_DIR="${PROJECT_ROOT}/tools"
WORK_DIR="${TOOLS_DIR}/_fontbuild"
ADAFRUIT_GFX_DIR="${WORK_DIR}/Adafruit-GFX-Library"
FONTSUBDIR="fontconvert"
FONTCVT="${ADAFRUIT_GFX_DIR}/${FONTSUBDIR}/fontconvert"

usage() {
  cat <<EOF
Usage: $0 [-f /chemin/police.ttf] [-s taille] [-o NomSortie.h]

Options:
  -f  Chemin vers le fichier .ttf (sinon tentative de téléchargement DejaVu Sans)
  -s  Taille en points (défaut: ${POINT_SIZE})
  -o  Nom du header généré (défaut: ${OUTPUT_NAME})
  -h  Aide

Exemple:
  $0 -f ~/Library/Fonts/DejaVuSans.ttf
  $0 -s 10 -o CustomSans10ptLat1.h
EOF
}

while getopts "f:s:o:h" opt; do
  case $opt in
    f) FONT_TTF="$OPTARG" ;;
    s) POINT_SIZE="$OPTARG" ;;
    o) OUTPUT_NAME="$OPTARG" ;;
    h) usage; exit 0 ;;
    *) usage; exit 1 ;;
  esac
done

echo "[INFO] Racine projet: ${PROJECT_ROOT}"
mkdir -p "${WORK_DIR}"

# 1. Récupération / compilation fontconvert
if [[ ! -x "${FONTCVT}" ]]; then
  echo "[INFO] Clonage Adafruit-GFX-Library..."
  rm -rf "${ADAFRUIT_GFX_DIR}" || true
  git clone --depth=1 https://github.com/adafruit/Adafruit-GFX-Library.git "${ADAFRUIT_GFX_DIR}"
  echo "[INFO] Détection FreeType..."
  FT_PREFIX=""
  if command -v brew >/dev/null 2>&1; then
    for f in freetype freetype2; do
      if brew --prefix "$f" >/dev/null 2>&1; then FT_PREFIX="$(brew --prefix "$f")"; break; fi
    done
  fi
  EXTRA_CFLAGS=""
  LDFLAGS_VAR=""
  if [[ -n "$FT_PREFIX" ]]; then
    if [[ -d "$FT_PREFIX/include/freetype2" ]]; then
      EXTRA_CFLAGS+=" -I$FT_PREFIX/include/freetype2"
    fi
    if [[ -d "$FT_PREFIX/lib" ]]; then
      LDFLAGS_VAR+=" -L$FT_PREFIX/lib -Wl,-rpath,$FT_PREFIX/lib"
    fi
    echo "[INFO] FreeType path: $FT_PREFIX"
  fi
  if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists freetype2; then
    PKG_CFLAGS="$(pkg-config --cflags freetype2)"
    PKG_LIBS="$(pkg-config --libs freetype2)"
    EXTRA_CFLAGS+=" $PKG_CFLAGS"
    LDFLAGS_VAR+=" $PKG_LIBS"
    echo "[INFO] pkg-config freetype2: $PKG_CFLAGS | $PKG_LIBS"
  fi
  echo "[INFO] Compilation fontconvert (directe) ..."
  (cd "${ADAFRUIT_GFX_DIR}/${FONTSUBDIR}" && \
     gcc -Wall $EXTRA_CFLAGS fontconvert.c $LDFLAGS_VAR -lfreetype -o fontconvert 2>/dev/null || \
     gcc -Wall $EXTRA_CFLAGS fontconvert.c $LDFLAGS_VAR "$FT_PREFIX/lib/libfreetype.dylib" -o fontconvert ) || { echo "[ERREUR] Échec compilation fontconvert"; exit 3; }
  if [[ ! -x "${FONTCVT}" ]]; then
    echo "[ERREUR] fontconvert non généré"; exit 4
  fi
fi

# 2. Police source
if [[ -z "${FONT_TTF}" ]]; then
  for p in \
    "${HOME}/Library/Fonts/DejaVuSans.ttf" \
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf" \
    "/usr/local/share/fonts/DejaVuSans.ttf"; do
    if [[ -f "$p" ]]; then FONT_TTF="$p"; break; fi
  done
fi

if [[ -z "${FONT_TTF}" ]]; then
  echo "[INFO] Téléchargement DejaVu Sans..."
  TMPZIP="${WORK_DIR}/dejavu.zip"
  curl -L -o "$TMPZIP" https://github.com/dejavu-fonts/dejavu-fonts/releases/download/version_2_37/dejavu-fonts-ttf-2.37.zip
  unzip -qo "$TMPZIP" -d "${WORK_DIR}/dejavu"
  FONT_TTF=$(find "${WORK_DIR}/dejavu" -type f -name "DejaVuSans.ttf" -print -quit || true)
fi

if [[ ! -f "${FONT_TTF}" ]]; then
  echo "[ERREUR] Police TTF introuvable." >&2
  exit 2
fi
echo "[INFO] Utilisation TTF: ${FONT_TTF}"

# 3. Génération
OUT_TMP="${WORK_DIR}/${OUTPUT_NAME}"
echo "[INFO] Génération ${OUTPUT_NAME} (plage ${FIRST_CODE}-${LAST_CODE}, ${POINT_SIZE}pt)"
"${FONTCVT}" "${FONT_TTF}" "${POINT_SIZE}" "${FIRST_CODE}" "${LAST_CODE}" > "${OUT_TMP}"

# 4. Renommage symbole interne probable (ex: DejaVuSans9pt7b -> DejaVuSans9ptLat1)
ORIG_SYM="DejaVuSans${POINT_SIZE}pt7b"
NEW_SYM_BASE="${OUTPUT_NAME%.h}"

if grep -q "${ORIG_SYM}" "${OUT_TMP}"; then
  echo "[INFO] Renommage symbole ${ORIG_SYM} -> ${NEW_SYM_BASE}"
  if [[ "$OSTYPE" == "darwin"* ]]; then
    sed -i '' "s/${ORIG_SYM}/${NEW_SYM_BASE}/g" "${OUT_TMP}"
  else
    sed -i "s/${ORIG_SYM}/${NEW_SYM_BASE}/g" "${OUT_TMP}"
  fi
fi

# 5. Ajout garde inclusion si absent
if ! grep -q "#ifndef" "${OUT_TMP}"; then
  GUARD="$(echo "${NEW_SYM_BASE}" | tr '[:lower:]' '[:upper:]' | tr -c 'A-Z0-9' '_')_H"
  { echo "#ifndef ${GUARD}"; echo "#define ${GUARD}"; cat "${OUT_TMP}"; echo "#endif // ${GUARD}"; } > "${OUT_TMP}.guard" && mv "${OUT_TMP}.guard" "${OUT_TMP}" 
fi

# 6. Sauvegarde ancienne version si présente
DEST_FILE="${EXAMPLES_DIR}/${OUTPUT_NAME}"
if [[ -f "${DEST_FILE}" ]]; then
  cp -f "${DEST_FILE}" "${DEST_FILE}.bak.$(date +%Y%m%d-%H%M%S)"
  echo "[INFO] Backup ancienne police -> ${DEST_FILE}.bak.*"
fi

cp "${OUT_TMP}" "${DEST_FILE}"
echo "[OK] Police générée: ${DEST_FILE}"
echo "[NOTE] Recompiler: pio run -e fullscreen_countdown_web"

exit 0
