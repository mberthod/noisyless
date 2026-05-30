#!/bin/bash
set -euo pipefail

# ============================================================
# NOISYLESS — Script de flash production
# Usage:
#   ./flash-noisyless.sh                           # flash /dev/ttyACM0 (erase + flash)
#   ./flash-noisyless.sh /dev/ttyUSB0               # flash port spécifique
#   ./flash-noisyless.sh --no-erase                 # sans erase (OTA recovery)
#   ./flash-noisyless.sh --all                      # tous les modules
#   ./flash-noisyless.sh --build                    # compile puis flash
#   ./flash-noisyless.sh --fw v0.0.1                # flash une version spécifique existante
#   ./flash-noisyless.sh --list                     # liste les ports
# ============================================================

FIRMWARE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_DIR="$FIRMWARE_DIR/05_FIRMWARE"
BUILD_DIR="$PROJECT_DIR/.pio/build/esp32dev"
BOOTLOADER="$BUILD_DIR/bootloader.bin"
PARTITIONS="$BUILD_DIR/partitions.bin"
VENV="/home/mathieu/.pio-venv/bin/activate"
ESPPORT="${ESP_PORT:-/dev/ttyACM0}"
BAUD=921600
DO_ERASE=true

# Détection auto de la version depuis main.cpp
detect_version() {
    local fw_file="$PROJECT_DIR/src/main.cpp"
    if [ -f "$fw_file" ]; then
        FW_VERSION=$(grep -oP 'FW_VERSION\s*=\s*"\K[^"]+' "$fw_file" 2>/dev/null || echo "inconnue")
    else
        FW_VERSION="inconnue"
    fi
    # Version spécifique ?
    if [ -n "${FW_OVERRIDE:-}" ]; then
        FW_VERSION="$FW_OVERRIDE"
        FIRMWARE_BIN="$BUILD_DIR/firmware.bin"
    else
        FIRMWARE_BIN="$BUILD_DIR/firmware.bin"
    fi
}

detect_version

# Couleurs
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Activer l'environnement PlatformIO
if [ -f "$VENV" ]; then
    source "$VENV"
fi

# Vérifier pio
if ! command -v pio &>/dev/null; then
    echo -e "${RED}❌ PlatformIO (pio) non trouvé${NC}"
    exit 1
fi

# Vérifier que le firmware est compilé
check_build() {
    if [ ! -f "$FIRMWARE_BIN" ]; then
        echo -e "${RED}❌ Firmware non compilé : $FIRMWARE_BIN${NC}"
        echo "   Lance d'abord : cd $PROJECT_DIR && pio run -e esp32dev"
        echo "   Ou : $(basename $0) --build"
        exit 1
    fi
    echo -e "${GREEN}✓ Firmware : $(stat --format='%s' "$FIRMWARE_BIN" | numfmt --to=iec)${NC}"
    echo -e "${GREEN}✓ Version  : $FW_VERSION${NC}"
}

flash_device() {
    local PORT="$1"
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}  NOISYLESS $FW_VERSION — Flash sur $PORT${NC}"
    echo -e "${YELLOW}========================================${NC}"

    # Effacement complet de la flash
    if [ "$DO_ERASE" = true ]; then
        echo -e "${GREEN}▶ Effacement complet de la flash...${NC}"
        esptool.py --chip esp32s3 --port "$PORT" --baud "$BAUD" erase_flash 2>&1 | grep -E "Erasing|error|Error" || true
        echo -e "${GREEN}✅ Flash effacée (NVS + partitions + firmware)${NC}"
    fi

    # Bootloader + partitions + firmware
    echo -e "${GREEN}▶ Bootloader + Partitions + Firmware...${NC}"
    cd "$PROJECT_DIR" 2>/dev/null || true

    echo -e "${GREEN}▶ Firmware $FW_VERSION...${NC}"
    esptool.py --chip esp32s3 --port "$PORT" --baud "$BAUD" \
        --before default_reset --after hard_reset write_flash -z \
        --flash_mode dio --flash_freq 80m --flash_size 8MB \
        0x0 "$BOOTLOADER" \
        0x8000 "$PARTITIONS" \
        0x10000 "$FIRMWARE_BIN" 2>&1 | grep -E "Writing at|Hash|Hard reset|Leaving|error|Error" || true

    echo -e "${GREEN}✅ Flash terminé sur $PORT${NC}"
    if [ "$DO_ERASE" = true ]; then
        echo -e "${YELLOW}   • Flash effacée + firmware $FW_VERSION écrit${NC}"
        echo -e "${YELLOW}   • NVS réinitialisée${NC}"
    else
        echo -e "${YELLOW}   • Firmware $FW_VERSION flashé (sans erase)${NC}"
    fi
    echo -e "${YELLOW}   • Device ID unique (base MAC)${NC}"
    echo -e "${YELLOW}   • LED : rouge boot → bleu WiFi → vert OK${NC}"
    echo -e "${YELLOW}   • WiFi : WiFiManager captive portal \"NOISYLESS-Setup\"${NC}"
    echo -e "${YELLOW}   • Fallback : SSID='TP-Link_B150' hardcodé${NC}"
    echo -e "${YELLOW}   • MQTT : 91.99.26.43:1883 (user: esp32)${NC}"
    echo -e "${YELLOW}   • OTA auto : toutes les 60s${NC}"
    echo ""
}

# Help
if [ "${1:-}" = "--help" ] || [ "${1:-}" = "-h" ]; then
    FW_DISPLAY="$FW_VERSION"
    echo -e "${GREEN}NOISYLESS — Script de flash production${NC}"
    echo ""
    echo "Usage:"
    echo "  $(basename $0)                        Flash /dev/ttyACM0 (erase + flash)"
    echo "  $(basename $0) /dev/ttyUSB0           Flash port spécifique (erase + flash)"
    echo "  $(basename $0) --fw vX.Y.Z            Flash version spécifique (ex: v0.0.1)"
    echo "  $(basename $0) --no-erase              Flash sans erase (OTA recovery)"
    echo "  $(basename $0) --all                   Flash tous les modules branchés"
    echo "  $(basename $0) --list                  Liste les ports disponibles"
    echo "  $(basename $0) --build                 Compile puis flash"
    echo ""
    echo "Dernière version compilée : $FW_DISPLAY"
    echo "Variable d'env: ESP_PORT pour changer le port par défaut"
    exit 0
fi

# List
if [ "${1:-}" = "--list" ]; then
    echo -e "${GREEN}Ports série disponibles :${NC}"
    ls -1 /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || echo "  Aucun"
    exit 0
fi

# Version spécifique
if [ "${1:-}" = "--fw" ]; then
    FW_OVERRIDE="${2:-}"
    if [ -z "$FW_OVERRIDE" ]; then
        echo -e "${RED}❌ Spécifie une version : --fw v0.0.1${NC}"
        exit 1
    fi
    # Chercher le binaire dans le build dir
    VERSION_BIN="$BUILD_DIR/firmware.bin"
    if [ ! -f "$VERSION_BIN" ]; then
        echo -e "${RED}❌ Binaire $VERSION_BIN introuvable. Compile d'abord avec --build${NC}"
        exit 1
    fi
    detect_version
    shift 2
    # Re-entry avec les bons args
    set -- "$@"
    # Vérifier si --no-erase dans les args restants
    if [ "${1:-}" = "--no-erase" ]; then DO_ERASE=false; PORT="${2:-$ESPPORT}"
    elif [ -n "${1:-}" ] && [ "${1:0:1}" != "-" ]; then PORT="${1:-$ESPPORT}"
    else PORT="$ESPPORT"
    fi
    check_build
    if [ ! -e "$PORT" ]; then
        echo -e "${RED}❌ Port $PORT introuvable. Ports disponibles :${NC}"
        ls -1 /dev/ttyACM* /dev/ttyUSB* 2>/dev/null; exit 1
    fi
    flash_device "$PORT"
    echo -e "${GREEN}🎉 Flash $FW_OVERRIDE terminé !${NC}"
    exit 0
fi

# Build + flash
if [ "${1:-}" = "--build" ]; then
    echo -e "${GREEN}▶ Compilation...${NC}"
    cd "$PROJECT_DIR"
    pio run -e esp32dev 2>&1 | grep -E "Success|Error|error" | tail -5
    cd - > /dev/null
    detect_version
    check_build
    PORT="${2:-$ESPPORT}"
    flash_device "$PORT"
    exit 0
fi

# All
if [ "${1:-}" = "--all" ]; then
    DO_ERASE=true
    if [ "${2:-}" = "--no-erase" ]; then DO_ERASE=false; fi
    check_build
    PORTS=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true)
    if [ -z "$PORTS" ]; then
        echo -e "${RED}❌ Aucun port série trouvé${NC}"
        exit 1
    fi
    for PORT in $PORTS; do
        flash_device "$PORT"
    done
    echo -e "${GREEN}🎉 Tous les modules flashés !${NC}"
    exit 0
fi

# Single port
if [ "${1:-}" = "--no-erase" ]; then
    DO_ERASE=false
    PORT="${2:-$ESPPORT}"
elif [ -n "${1:-}" ] && [ "${1:0:1}" != "-" ]; then
    PORT="${1:-$ESPPORT}"
else
    PORT="$ESPPORT"
fi

check_build
if [ ! -e "$PORT" ]; then
    echo -e "${RED}❌ Port $PORT introuvable. Ports disponibles :${NC}"
    ls -1 /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
    exit 1
fi
flash_device "$PORT"
echo -e "${GREEN}🎉 Flash terminé !${NC}"
