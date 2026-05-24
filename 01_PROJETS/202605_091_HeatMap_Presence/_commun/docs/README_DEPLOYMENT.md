# NOISYLESS — Documentation de Déploiement

## 📋 Vue d'Ensemble

**Architecture:**
```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Nginx     │────▶│   API       │────▶│ TimescaleDB │
│   (443)     │     │ (FastAPI)   │     │  (Postgres) │
└─────────────┘     └─────────────┘     └─────────────┘
       │                   │
       │            ┌──────┴──────┐
       │            │   Mosquitto │
       │            │   (MQTT)    │
       │            └─────────────┘
       │
┌──────┴──────┐
│   Static    │
│   (HTML)    │
└─────────────┘
```

**Hébergement:**
- **VPS:** OVH France (91.99.26.43)
- **DNS/Proxy:** Cloudflare
- **Domaine:** noisyless.com, platform.noisyless.com
- **SMTP:** OVH (contact@noisyless.com)
- **Paiement:** Stripe LIVE

---

## 🚀 Déploiement Rapide

### 1. Prérequis

```bash
# SSH vers VPS
ssh noisyless@91.99.26.43

# Docker installé + Docker Compose
docker --version
docker compose version
```

### 2. Configuration

```bash
cd /opt/noisyless

# Éditer .env
nano .env
```

**Variables requises:**
```env
# Database
DB_PASSWORD=noisyless_secret_password

# JWT
JWT_SECRET=noisyless_jwt_super_secret_key_2026

# Stripe (OBLIGATOIRE - clés LIVE)
STRIPE_PUBLISHABLE_KEY=pk_live_...
STRIPE_SECRET_KEY=sk_live_...

# OVH SMTP
SMTP_HOST=ssl0.ovh.net
SMTP_PORT=465
SMTP_USER=contact@noisyless.com
SMTP_PASSWORD=your_password

# Telegram (optionnel)
TELEGRAM_BOT_TOKEN=
TELEGRAM_CHAT_ID=
```

### 3. Démarrage

```bash
cd /opt/noisyless
docker compose build
docker compose up -d
```

### 4. Vérification

```bash
# Status containers
docker compose ps

# Logs API
docker compose logs -f api

# Test health
curl http://localhost/health
```

---

## 🛠️ Services

### API (FastAPI)

**Port:** 8000 (interne)  
**Routes:**
- `/auth/*` — Authentification
- `/api/devices` — Gestion appareils
- `/shop/*` — Stripe Checkout
- `/health` — Healthcheck

**Commandes:**
```bash
# Restart API
docker compose restart api

# Logs
docker compose logs -f api

# Shell dans container
docker compose exec api bash
```

### Nginx

**Ports:** 80, 443  
**Config:** `/opt/noisyless/nginx/nginx.conf`

**Routes:**
- `/` → Static files
- `/api/` → API backend
- `/auth/` → API backend
- `/shop/` → API backend (Stripe)
- `/health` → API backend

**Reload:**
```bash
docker compose exec nginx nginx -s reload
```

### Mosquitto (MQTT)

**Ports:** 1883 (non-SSL), 8883 (SSL)  
**Config:** `/opt/noisyless/mosquitto/config/`

### TimescaleDB

**Port:** 5432 (localhost only)  
**User:** noisyless  
**Database:** noisyless

**Backup:**
```bash
docker compose exec timescaledb pg_dump -U noisyless noisyless > backup.sql
```

---

## 💳 Stripe Checkout

### Configuration

1. **Récupérer clés LIVE:** https://dashboard.stripe.com/apikeys
2. **Ajouter dans .env:**
   ```env
   STRIPE_PUBLISHABLE_KEY=pk_live_...
   STRIPE_SECRET_KEY=sk_live_...
   ```
3. **Restart API:**
   ```bash
   docker compose restart api
   ```

### Test

```bash
curl -X POST https://platform.noisyless.com/shop/checkout \
  -H "Content-Type: application/json" \
  -d '{
    "items": [{"product_id": "nl-simple", "quantity": 1}],
    "email": "test@test.com",
    "name": "Test",
    "success_url": "https://noisyless.com/merci",
    "cancel_url": "https://noisyless.com/annule"
  }'
```

**Réponse succès:**
```json
{
  "session_id": "cs_live_...",
  "url": "https://checkout.stripe.com/c/pay/cs_live_..."
}
```

### Webhook (à implémenter)

**Endpoint:** `POST /shop/webhook`  
**Événements:**
- `checkout.session.completed` — Paiement réussi
- `checkout.session.expired` — Session expirée
- `payment_intent.payment_failed` — Échec paiement

---

## 📧 Emails (OVH SMTP)

### Configuration

```env
SMTP_HOST=ssl0.ovh.net
SMTP_PORT=465
SMTP_USER=contact@noisyless.com
SMTP_PASSWORD=your_password
SMTP_FROM="NOISYLESS <contact@noisyless.com>"
```

### Test

```bash
openssl s_client -connect ssl0.ovh.net:465
EHLO noisyless.com
AUTH LOGIN
# encoder base64: echo -n "contact@noisyless.com" | base64
MAIL FROM: <contact@noisyless.com>
RCPT TO: <test@example.com>
DATA
Subject: Test
Test email
.
QUIT
```

---

## 🔐 Sécurité

### Firewall (UFW)

```bash
# Sur VPS
ufw default deny incoming
ufw allow 22/tcp      # SSH
ufw allow 80/tcp      # HTTP
ufw allow 443/tcp     # HTTPS
ufw allow 1883/tcp    # MQTT
ufw enable
```

### Fail2ban

```bash
apt install fail2ban
systemctl enable fail2ban
```

### SSL/TLS

Certificats dans `/opt/noisyless/nginx/ssl/`  
Renouvellement: manuel (Let's Encrypt ou OVH)

### Rotation Clés

- **Stripe:** Tous les 90 jours
- **JWT:** En cas de compromission
- **DB Password:** Jamais (sauf fuite)

---

## 📊 Monitoring

### Logs

```bash
# Tous services
docker compose logs -f

# API seulement
docker compose logs -f api

# Dernières 100 lignes
docker compose logs --tail=100 api
```

### Healthcheck

```bash
# API
curl https://platform.noisyless.com/health

# Database
docker compose exec timescaledb pg_isready -U noisyless
```

### Metrics

- **CPU/RAM:** `docker stats`
- **Disk:** `df -h`
- **Network:** `iftop` ou `nethogs`

---

## 🔄 Mises à Jour

### Code

```bash
cd /opt/noisyless
git pull
docker compose build
docker compose up -d
```

### Base de Données

```bash
# Migration scripts dans /opt/noisyless/timescaledb/init/
# Exécutés automatiquement au premier démarrage
```

### Rollback

```bash
# Revenir version précédente
git checkout <commit-hash>
docker compose build
docker compose up -d
```

---

## 🐛 Debugging

### API ne démarre pas

```bash
# Voir logs
docker compose logs api

# Vérifier .env
docker compose exec api env | grep STRIPE

# Tester connexion DB
docker compose exec api python -c "import asyncpg; print('OK')"
```

### Stripe rejeté

```bash
# Vérifier clé
docker compose exec api python -c "import stripe, os; stripe.api_key = os.getenv('STRIPE_SECRET_KEY'); print(stripe.Balance.retrieve())"

# Tester avec curl
curl -u "sk_live_...:" https://api.stripe.com/v1/products
```

### Nginx 502

```bash
# Vérifier API up
docker compose ps api

# Voir logs nginx
docker compose logs nginx

# Tester en direct
curl http://api:8000/health
```

---

## 📁 Structure Fichiers

```
/opt/noisyless/
├── docker-compose.yml
├── .env
├── services/
│   ├── api/
│   │   ├── main.py
│   │   ├── shop.py
│   │   ├── devices.py
│   │   ├── auth.py
│   │   └── requirements.txt
│   ├── mqtt-ingest/
│   ├── heatmap-agg/
│   └── alert-engine/
├── nginx/
│   ├── nginx.conf
│   └── ssl/
├── mosquitto/
│   └── config/
├── timescaledb/
│   ├── data/
│   └── init/
└── static/
    └── index.html
```

---

## 📞 Support

| Issue | Contact |
|-------|---------|
| Stripe | support@stripe.com |
| OVH | support.ovh.com |
| Cloudflare | dash.cloudflare.com |
| VPS | ssh noisyless@91.99.26.43 |

---

**Dernière mise à jour:** 24 Mai 2026
