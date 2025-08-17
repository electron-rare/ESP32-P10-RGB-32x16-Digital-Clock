#!/usr/bin/env bash
set -euo pipefail

# Script pour générer les polices DejaVu Bold et Oblique
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS_DIR="${PROJECT_ROOT}/tools"
WORK_DIR="${TOOLS_DIR}/_fontbuild"
DEJAVU_DIR="${WORK_DIR}/dejavu/dejavu-fonts-ttf-2.37/ttf"
EXAMPLES_DIR="${PROJECT_ROOT}/examples"

# Vérifier que les TTF existent
if [[ ! -f "${DEJAVU_DIR}/DejaVuSans-Bold.ttf" ]]; then
    echo "Erreur: DejaVuSans-Bold.ttf non trouvé dans ${DEJAVU_DIR}"
    exit 1
fi

if [[ ! -f "${DEJAVU_DIR}/DejaVuSans-Oblique.ttf" ]]; then
    echo "Erreur: DejaVuSans-Oblique.ttf non trouvé dans ${DEJAVU_DIR}"
    exit 1
fi

echo "Génération des polices DejaVu Bold et Oblique..."

# Utiliser le script existant pour générer Bold
echo "Génération DejaVu Bold..."
"${TOOLS_DIR}/generate_latin1_font.sh" -f "${DEJAVU_DIR}/DejaVuSans-Bold.ttf" -o "DejaVuSansBold9ptLat1.h"

# Utiliser le script existant pour générer Oblique  
echo "Génération DejaVu Oblique..."
"${TOOLS_DIR}/generate_latin1_font.sh" -f "${DEJAVU_DIR}/DejaVuSans-Oblique.ttf" -o "DejaVuSansOblique9ptLat1.h"

echo "Polices générées avec succès!"
echo "- ${EXAMPLES_DIR}/DejaVuSansBold9ptLat1.h"
echo "- ${EXAMPLES_DIR}/DejaVuSansOblique9ptLat1.h"
