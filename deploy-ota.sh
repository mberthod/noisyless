#!/bin/bash
# Script de déploiement OTA pour Noisyless
# Usage: ./deploy-ota.sh <VERSION> [description]

set -e

if [ -z "$1" ]; then
  echo "Usage: $0 <version> [changelog]"
  echo "Exemple: $0 0.0.4 \"Bug fixes and LED version display\""
  exit 1
fi

VERSION="$1"
CHANGELOG="${2:-No description provided}"

PROJECT_ROOT="/data/home-mathieu/noisyless"
FIRMWARE_DIR="$PROJECT_ROOT/05_FIRMWARE"
VPS_USER="noisyless"
VPS_HOST="91.99.26.43"
VPS_PATH="/opt/noisyless/static/firmware"

echo "=== Déploiement OTA Noisyless v$VERSION ==="

# 1. Compiler le firmware
echo "→ Compilation..."
cd "$FIRMWARE_DIR"
pio run -e esp32dev

# 2. Générer le SHA256
echo "→ Calcul du SHA256..."
FIRMWARE_BIN="$FIRMWARE_DIR/.pio/build/esp32dev/firmware.bin"
SHA256=$(sha256sum "$FIRMWARE_BIN" | cut -d' ' -f1)
FILENAME="noisyless-v${VERSION}.bin"

echo "  SHA256: $SHA256"

# 3. Copier le binaire sur le VPS
echo "→ Upload sur le VPS..."
scp "$FIRMWARE_BIN" "$VPS_USER@$VPS_HOST:/opt/noisyless/static/firmware/$FILENAME.tmp"
ssh "$VPS_USER@$VPS_HOST" "mv /opt/noisyless/static/firmware/$FILENAME.tmp $VPS_PATH/$FILENAME && chmod 644 $VPS_PATH/$FILENAME"

# 4. Mettre à jour le manifest stable.json
echo "→ Mise à jour du manifest stable.json..."
ssh "$VPS_USER@$VPS_HOST" "cat > $VPS_PATH/stable.json << EOF
{
  \"latest\": \"$VERSION\",
  \"min\": \"0.0.0\",
  \"bin\": \"https://noisyless.com/firmware/$FILENAME\",
  \"sha256\": \"$SHA256\",
  \"changelog\": \"$CHANGELOG\"
}
EOF"

# 5. Commit git
echo "→ Commit git..."
cd "$PROJECT_ROOT"
git add "$FIRMWARE_DIR/src/main.cpp"
git commit -m "firmware v$VERSION: $CHANGELOG"

echo ""
echo "=== Déploiement terminé ==="
echo "Les modules se mettront à jour automatiquement (check OTA toutes les 60s)"
echo "ou au reboot. Pour forcer : débranche/rebranche l'alim."
