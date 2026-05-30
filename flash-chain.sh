#!/bin/bash
set -euo pipefail

# ============================================================
# NOISYLESS — Flash en chaîne (hotplug USB)
# Usage:
#   ./flash-chain.sh                     # Attend un module, flash, attend le suivant...
#   ./flash-chain.sh --keep-flashing      # Flash en boucle infinie
# ============================================================

FIRMWARE_DIR="/data/home-mathieu/noisyless/05_FIRMWARE"
BUILD_DIR="$FIRMWARE_DIR/.pio/build/esp32dev"
BOOTLOADER="$BUILD_DIR/bootloader.bin"
PARTITIONS="$BUILD_DIR/partitions.bin"
FIRMWARE_BIN="$BUILD_DIR/firmware.bin"
BAUD=921600

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

flashed=0
keep_going=false
[ "${1:-}" = "--keep-flashing" ] && keep_going=true

# Vérifier firmware compilé
if [ ! -f "$FIRMWARE_BIN" ]; then
    echo -e "${RED}❌ Firmware non compilé. Lance: pio run dans $FIRMWARE_DIR${NC}"
    exit 1
fi

FW_VERSION=$(grep -oP 'FW_VERSION\s*=\s*"\K[^"]+' "$FIRMWARE_DIR/src/main.cpp" 2>/dev/null || echo "?")
echo -e "${GREEN}Firmware: v$FW_VERSION (${FIRMWARE_BIN##*/})${NC}"
echo -e "${YELLOW}Attente d'un module ESP32 à brancher...${NC}"
echo ""

known_ports=()

scan_ports() {
    ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true
}

flash_one() {
    local port="$1"
    local mac=""

    echo ""
    echo -e "${CYAN}═══════════════════════════════════════${NC}"
    echo -e "${CYAN}  🔌 Module détecté : $port${NC}"

    # Lire le MAC
    sleep 1
    mac=$(esptool.py --port "$port" read_mac 2>/dev/null | grep "MAC:" | tail -1 | awk '{print $2}')
    echo -e "${CYAN}  MAC : $mac${NC}"
    echo -e "${CYAN}═══════════════════════════════════════${NC}"

    # Effacement complet
    echo -e "${YELLOW}▶ Erase flash...${NC}"
    esptool.py --chip esp32s3 --port "$port" --baud "$BAUD" erase_flash 2>&1 | grep -E "Chip erase|error" || true

    # Flash bootloader + partitions + firmware
    echo -e "${YELLOW}▶ Flash bootloader...${NC}"
    esptool.py --chip esp32s3 --port "$port" --baud "$BAUD" \
        --before default_reset --after hard_reset write_flash -z \
        --flash_mode dio --flash_freq 40m --flash_size 8MB \
        0x0 "$BOOTLOADER" \
        0x8000 "$PARTITIONS" \
        0x10000 "$FIRMWARE_BIN" 2>&1 | grep -E "Writing at|Hash|Hard reset|Leaving|error|100" || true

    flashed=$((flashed + 1))
    echo ""
    echo -e "${GREEN}✅ Flash #$flashed terminé — v$FW_VERSION → $mac${NC}"
    echo -e "${YELLOW}   → LED: 10 flashs bleus séquence de boot${NC}"
    echo -e "${YELLOW}   → WiFi: captive portal 'NOISYLESS-Setup'${NC}"
    echo -e "${YELLOW}   → OTA: check automatique toutes les 2min${NC}"
    echo ""
    echo -e "${YELLOW}Module prêt, attente du suivant...${NC}"
}

while true; do
    current_ports=($(scan_ports))

    for port in "${current_ports[@]}"; do
        if [[ ! " ${known_ports[*]:-} " =~ " $port " ]]; then
            flash_one "$port"
            known_ports+=("$port")
        fi
    done

    # Si un port disparaît, le retirer de la liste connue
    new_known=()
    for port in "${known_ports[@]}"; do
        if [[ " ${current_ports[*]:-} " =~ " $port " ]]; then
            new_known+=("$port")
        fi
    done
    known_ports=("${new_known[@]:-}")

    if [ "$keep_going" = false ] && [ "$flashed" -ge 11 ]; then
        echo -e "${GREEN}🎉 11 modules flashés — terminé !${NC}"
        break
    fi

    sleep 2
done
