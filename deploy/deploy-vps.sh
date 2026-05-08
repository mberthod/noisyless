#!/bin/bash
# Deploiement MQTT ingest + Mosquitto sur VPS NOISYLESS
# Usage: bash deploy-vps.sh
set -euo pipefail

VPS="noisyless@91.99.26.43"
REMOTE_DIR="/opt/noisyless"

echo "=== NOISYLESS MQTT Deploy ==="

# 1. Upload mqtt-ingest service
echo "[1/6] Upload mqtt-ingest..."
ssh "$VPS" "mkdir -p $REMOTE_DIR/mqtt-ingest"
scp mqtt-ingest/mqtt_ingest.py mqtt-ingest/Dockerfile mqtt-ingest/requirements.txt \
    "$VPS:$REMOTE_DIR/mqtt-ingest/"

# 2. Upload mosquitto config (fixed)
echo "[2/6] Upload mosquitto config..."
ssh "$VPS" "mkdir -p $REMOTE_DIR/mosquitto/config"
scp mosquitto/mosquitto.conf "$VPS:$REMOTE_DIR/mosquitto/config/mosquitto.conf"

# 3. Create mosquitto passwd file (if not exists)
echo "[3/6] Create MQTT passwd file..."
ssh "$VPS" "
    if [ ! -f $REMOTE_DIR/mosquitto/config/passwd ]; then
        docker run --rm -v $REMOTE_DIR/mosquitto/config:/mosquitto/config \
            eclipse-mosquitto:2 \
            mosquitto_passwd -b -c /mosquitto/config/passwd esp32 'am#k^keu5nBDWLB'
        echo 'passwd created for user esp32'
    else
        echo 'passwd already exists'
    fi
"

# 4. Patch docker-compose.yml to add mqtt-ingest + fix mosquitto
echo "[4/6] Patching docker-compose.yml..."
ssh "$VPS" "cat $REMOTE_DIR/docker-compose.yml" > /tmp/noisyless-compose-backup.yml
echo "  Backup saved to /tmp/noisyless-compose-backup.yml"

# Generate the patched compose with mqtt-ingest added + mosquitto volumes fixed
ssh "$VPS" "cat > $REMOTE_DIR/docker-compose.mqtt-ingest.yml << 'COMPOSE_EOF'
# Overlay: mqtt-ingest service
services:
  mqtt-ingest:
    build: ./mqtt-ingest
    container_name: noisyless-mqtt-ingest
    restart: unless-stopped
    environment:
      MQTT_HOST: mosquitto
      MQTT_PORT: \"1883\"
      MQTT_USER: esp32
      MQTT_PASS: \"am#k^keu5nBDWLB\"
      MQTT_TOPIC: \"esp32/noisyless/+/datas\"
      DATABASE_URL: \${DATABASE_URL}
    depends_on:
      - mosquitto
      - timescaledb
COMPOSE_EOF
echo 'mqtt-ingest overlay created'
"

# 5. Fix mosquitto in main compose: volumes + ports
echo "[5/6] Fixing mosquitto service in compose..."
ssh "$VPS" "
cd $REMOTE_DIR

# Fix mosquitto volumes (remove cert double-mount, add port 1883)
python3 -c \"
import re
with open('docker-compose.yml') as f:
    content = f.read()

# Fix mosquitto volumes: remove the LE cert mount that overwrites config
content = content.replace(
    '      - /etc/letsencrypt/live/\\\${DOMAIN}:/mosquitto/config:ro',
    '      - /etc/letsencrypt/live/\\\${DOMAIN}:/mosquitto/certs:ro')

# Add port 1883 if not present
if '1883:1883' not in content:
    content = content.replace(
        '      - \\\"8883:8883\\\"',
        '      - \\\"1883:1883\\\"\\n      - \\\"8883:8883\\\"')

with open('docker-compose.yml', 'w') as f:
    f.write(content)
print('docker-compose.yml patched')
\"
"

# 6. Start services
echo "[6/6] Starting mosquitto + mqtt-ingest..."
ssh "$VPS" "
cd $REMOTE_DIR
docker compose -f docker-compose.yml -f docker-compose.mqtt-ingest.yml up -d --build mosquitto mqtt-ingest
docker compose -f docker-compose.yml -f docker-compose.mqtt-ingest.yml ps mosquitto mqtt-ingest
"

echo ""
echo "=== DEPLOY DONE ==="
echo "Mosquitto: plain 1883 + TLS 8883"
echo "MQTT user: esp32"
echo "Ingest: subscribed to esp32/noisyless/+/datas"
echo ""
echo "Next: update firmware credentials.h MQTT_HOST to 91.99.26.43"
