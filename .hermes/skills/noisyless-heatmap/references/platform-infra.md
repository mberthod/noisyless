# Platform Infrastructure — NOISYLESS

## Serveur Hetzner CAX11
- **IP** : 91.99.26.43
- **SSH** : `noisyless@91.99.26.43`
- **Password** : `eUq^fJst9!b9#^UT#WRM`
- **Code** : `/opt/noisyless/`
- **Docker Compose** : `/opt/noisyless/docker-compose.yml`

## Troubleshooting accès serveur

### SSH "Connection refused" mais ping OK
Si `ssh: connect to host 91.99.26.43 port 22: Connection refused` mais que le serveur ping et que 80/443/1883 répondent → **sshd est crashé**. Le mot de passe root n'est PAS stocké dans nos fichiers. Solution :
1. Aller sur [console.hetzner.cloud](https://console.hetzner.cloud) → le projet → serveur CAX11
2. "Reset root password" → nouveau mot de passe envoyé par email
3. Ou activer le Rescue mode → mount disque → `chroot` → `passwd root`

### FastAPI crash loop (Restarting (1))
Si `docker ps` montre `noisyless-api` en `Restarting (1)` :
1. Vérifier les logs : `docker logs noisyless-api --tail 40`
2. Si `NameError: name 'HTTPBearer' is not defined` → import manquant dans un router
3. Fix : ajouter `from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials` dans le fichier concerné
4. Rebuild : `cd /opt/noisyless && docker compose build api && docker compose up -d api`
5. Vérifier : `docker ps --filter name=noisyless-api` → doit être `Up`

## Services Docker
| Service | Image | Port | Statut |
|---------|-------|------|--------|
| mosquitto | eclipse-mosquitto:2 | 1883, 8883 (TLS) | Up |
| timescaledb | timescale/timescaledb:latest-pg16 | 5432 (localhost only) | Up |
| api | FastAPI (Python) | 8000 (internal) | Up |
| nginx | nginx:alpine | 80, 443 | Up |
| mqtt-ingest | Custom | — | Up |
| heatmap-agg | Custom | — | Up |
| alert-engine | Custom | — | Up |

## Frontend
- **URL** : `https://platform.noisyless.com`
- **Fichiers** : `/opt/noisyless/static/` (served by nginx as `/var/www/static/`)
- **SPA** : Vue 3 CDN + Tailwind, single `app.js` (35 KB), `style.css`, `index.html`
- **API** : via nginx proxy `/api/` → `api:8000` (FastAPI)
- **Auth** : magic link (`/api/auth/send-magic-link`), JWT 30 min

⚠️ Le conteneur `frontend` (docker-compose) est une coquille vide — arrêté. Tout est servi en fichiers statiques.

## Database — TimescaleDB
**Schema** : `noisyless`
**Tables** :
- `users` (uuid PK, email UNIQUE, password_hash, name, profile_completed, account_type perso/pro)
- `auth_tokens` (token_hash, user_id FK, expires_at, used)
- `devices` (device_id PK varchar(20), name, last_seen, firmware_version)
- `user_devices` (user_id FK, device_id FK)
- `measurements` (TimescaleDB hypertable — device_id, time, temp, humidity, iaq, presence, db_avg, lux1, lux2)

## API Endpoints
**Existants** :
- `POST /api/auth/send-magic-link` → magic link par email
- `POST /api/auth/verify-magic-link` → JWT
- `GET /api/devices` → liste devices + dernière mesure
- `POST /api/devices` → register device
- `GET /api/devices/{id}/live` → metrics temps réel
- `GET /api/devices/{id}/timeline` → historique 24h
- `GET /api/health`

**À déployer** (code dans `/tmp/noisyless-api/routers/`) :
- `/api/provision` → QR code provisioning token
- `/api/provision/claim` → device claims token
- `/api/floor-plans` → CRUD plans d'étage
- `/api/rooms` → CRUD pièces
- `/api/device-positions` → positionnement capteurs sur plan
- `/api/admin/setup` → bootstrap admin user

## Environnement (.env)
```
DOMAIN=platform.noisyless.com
API_DOMAIN=api.platform.noisyless.com
ADMIN_EMAIL=contact@noisyless.com
DATABASE_URL=postgresql://noisyless:***@timescaledb:5432/noisyless
JWT_SECRET=DRQuqy...Zu@1
SMTP_HOST=ssl0.ovh.net
SMTP_PORT=587
SMTP_USER=noreply@noisyless.com
SMTP_FROM=NOISYLESS <noreply@noisyless.com>
```
## Nginx — règles proxy

- `platform.noisyless.com` → `/var/www/html/` (statique) + `/api/` → `api:8000`
- HTTP redirige vers HTTPS (sauf `.well-known/acme-challenge/` pour Let's Encrypt)
- Cert SSL : `/etc/letsencrypt/live/platform.noisyless.com/`
- `api.platform.noisyless.com` n'a **pas** de vhost dédié → utiliser `/api/` sur platform.

### CORS

Le site vitrine (`noisyless.com`) doit pouvoir appeler l'API (`platform.noisyless.com`) via `fetch()`. Sans headers CORS, le navigateur bloque. Configuration nginx pour `/api/` et `/shop/` :

```
location /api/ {
    add_header Access-Control-Allow-Origin "*" always;
    add_header Access-Control-Allow-Methods "GET, POST, OPTIONS" always;
    add_header Access-Control-Allow-Headers "Content-Type, Authorization" always;
    if ($request_method = OPTIONS) { return 204; }
    proxy_pass http://api_backend;
    ...
}
```

⚠️ **Ne pas utiliser `sed` pour ajouter ces headers** — `sed -i` successifs sur le serveur créent des doublons. Toujours partir de la source propre dans Kdrive (`_infra/nginx/nginx.conf`), éditer localement avec `patch`, puis `scp` vers le serveur et `docker compose restart nginx`.

**Test CORS** : `curl -sI -X OPTIONS -H 'Origin: https://noisyless.com' -H 'Access-Control-Request-Method: POST' http://localhost/shop/checkout | grep -i access-control`

## Troubleshooting

### SSH Connection Refused (mais ping OK, ports 80/443/1883 OK)
Symptôme : `ssh: connect to host 91.99.26.43 port 22: Connection refused` alors que le serveur ping et répond sur les ports applicatifs.
Cause : sshd est crashé ou firewall restreint.
**Solution** : le mot de passe root n'est **pas stocké** dans nos fichiers. Aller sur [console.hetzner.cloud](https://console.hetzner.cloud) → le projet → le serveur CAX11 → "Reset root password". Hetzner envoie le nouveau par email.

### API FastAPI en crash loop (Restarting)
Symptôme : `docker ps` montre `noisyless-api   Restarting (1) X seconds ago`.
Diagnostic : `docker logs noisyless-api --tail 40` pour voir l'erreur.
Cause la plus fréquente : `NameError: name 'HTTPBearer' is not defined` — un router utilise `HTTPBearer`/`HTTPAuthorizationCredentials` sans `from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials`.
**Fix** :
```bash
# 1. Corriger l'import manquant
sed -i 's|^from fastapi import .*|&\nfrom fastapi.security import HTTPBearer, HTTPAuthorizationCredentials|' /opt/noisyless/services/api/devices.py

# 2. Rebuild + relancer
cd /opt/noisyless && docker compose build api && docker compose up -d api

# 3. Vérifier
sleep 5 && docker ps --filter name=noisyless-api
```
