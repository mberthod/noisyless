#!/bin/bash
# Script to run on the remote server via SSH
# Usage: ssh -i ~/.ssh/id_ed25519 noisyless@91.99.26.43 'bash -s' < generate-ssl-remote.sh

set -e

cd /opt/noisyless

echo "=========================================="
echo "LET'S ENCRYPT SSL CERTIFICATE GENERATION"
echo "=========================================="
echo ""

# Create SSL directory if not exists
mkdir -p /opt/noisyless/nginx/ssl

echo "Starting certbot..."
echo ""

docker run --rm -it \
  -v /opt/noisyless/nginx/ssl:/etc/letsencrypt \
  certbot/certbot certonly --manual \
  -d platform.noisyless.com \
  -d api.platform.noisyless.com \
  --email contact@noisyless.com \
  --agree-tos \
  --preferred-challenges dns

echo ""
echo "=========================================="
echo "CERTIFICATE GENERATED SUCCESSFULLY!"
echo "=========================================="
echo ""
echo "Certificates location:"
echo "  /opt/noisyless/nginx/ssl/live/platform.noisyless.com/"
echo ""
echo "Files created:"
ls -la /opt/noisyless/nginx/ssl/live/platform.noisyless.com/
echo ""
echo "Restarting nginx..."
docker compose restart nginx
echo ""
echo "Testing HTTPS connection..."
curl -k https://platform.noisyless.com || echo "Note: curl failed, but nginx should be running"
echo ""
echo "Done!"
