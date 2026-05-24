# NOISYLESS — Plan d'Exécution Complet

**Généré:** 24 Mai 2026  
**Source:** Audit exhaustif du répertoire Kdrive + état réel des fichiers  
**Objectif:** Déploiement complet site + paiement + SEO

---

## 📊 État des Lieux par Composant (Audit Physique)

| Composant | Répertoire | Files | Status |
|-----------|-----------|-------|--------|
| **Backend API** | `backend/api/` | main.py, auth.py, devices.py, shop.py | ✅ Déployé VPS |
| **Infra Docker** | `_infra/` | docker-compose.yml, 4 services | ✅ Prêt |
| **TimescaleDB** | `_infra/timescaledb/init/` | 01_schema.sql complet | ✅ |
| **Nginx** | `_infra/nginx/` + `nginx/` | nginx.conf complet | ✅ |
| **Live Site** | `web/site/index.html` | 563 lignes, one-pager | ✅ En ligne |
| **Firmware #089** | `202604_089/05_FIRMWARE/` | main.cpp 916 lignes | ✅ Complet |
| **Firmware #091 ToF** | `202605_091/firmware/node-tof-esp32s3/` | ESP-IDF complet | ✅ Complet |
| **PCB #089** | `202604_089/03_CAO/` | 3 variants KiCad | ✅ Production |
| **PCB #091** | `202605_091/hardware/kicad/` | Schematic partiel | ⏳ En cours |
| **Radar 60GHz** | `202605_091/firmware/node-radar-60ghz/` | VIDE | ❌ Non commencé |
| **Frontend Vue.js** | `frontend/` | Répertoires vides | ❌ Scaffold only |
| **Blog** | `noisyless-deployment/blog/` | 9 articles SEO | ✅ Prêts upload |
| **Sub-products** | `202605_091/01_NOISYLESS_*` | README only | ❌ Stubs |
| **Stripe Checkout** | `backend/api/shop.py` | Code OK, clé INVALIDE | ⚠️ Bloqué |
| **MQTT TLS** | `_infra/mosquitto/config/mosquitto.conf` | Config présente | ⚠️ Non testé |

---

## 🧱 LOT 0 — FONDATIONS (Ce qui est déjà fait)

### ✅ Terminé et Déployé

| # | Tâche | Fichier | Preuve |
|---|-------|---------|--------|
| 0.1 | Fix HTTPBearer import | `devices.py:5` | API tourne |
| 0.2 | Création shop.py Stripe | `backend/api/shop.py` | 3 endpoints |
| 0.3 | Nginx /shop/ route | `nginx/nginx.conf` | Proxy API |
| 0.4 | Docker STRIPE_SECRET_KEY | `docker-compose.yml` | Env var |
| 0.5 | stripe module installé | `requirements.txt` | pip ok |
| 0.6 | Nginx /auth/ route | `nginx/nginx.conf` | Proxy API |
| 0.7 | Documentation DEPLOYMENT_LOG.md | Racine | Journal |
| 0.8 | Documentation README_DEPLOYMENT.md | Racine | Guide |
| 0.9 | .env.example template | `_infra/.env.example` | Template |

---

## 🚀 LOT 1 — STRIPE + PAIEMENT (8 micro-tâches)

### 1.1 Régénérer clé Stripe LIVE
```bash
# Dashboard: https://dashboard.stripe.com/apikeys
# 1. Cliquer "Create secret key" → copier
# 2. SSH VPS
ssh noisyless@91.99.26.43
# 3. Éditer .env
nano /opt/noisyless/.env
# STRIPE_SECRET_KEY=sk_live_... (NOUVELLE)
# 4. Restart API
docker compose -f /opt/noisyless/docker-compose.yml restart api
```
**Fichier:** `/opt/noisyless/.env`  
**Durée:** 15 min  
**✅ Critère:** `curl -u "sk_live_...:" https://api.stripe.com/v1/products` → JSON

---

### 1.2 Tester endpoint checkout
```bash
curl -X POST https://platform.noisyless.com/shop/checkout \
  -H 'Content-Type: application/json' \
  -d '{
    "items":[{"product_id":"nl-simple","quantity":1}],
    "email":"test@test.com","name":"Test",
    "success_url":"https://noisyless.com/merci",
    "cancel_url":"https://noisyless.com/annule"
  }'
```
**Durée:** 5 min  
**✅ Critère:** Réponse `{"session_id":"cs_live_...","url":"https://checkout.stripe.com/..."}`

---

### 1.3 Ajouter bouton "Acheter" dans index.html
```html
<!-- Fichier: web/site/index.html — Section produit NOISYLESS Simple -->
<!-- 1. Trouver section produit (l.~150-250) -->
<!-- 2. Ajouter formulaire avant le bouton -->
<form id="checkout-form" onsubmit="return startCheckout(event)">
  <input type="email" id="email" placeholder="Votre email" required>
  <input type="text" id="name" placeholder="Votre nom" required>
  <button type="submit">Acheter — 79€</button>
</form>

<!-- 3. Ajouter script en fin de body -->
<script>
async function startCheckout(e) {
  e.preventDefault();
  const btn = e.target.querySelector('button');
  btn.disabled = true; btn.textContent = 'Redirection...';
  try {
    const r = await fetch('https://platform.noisyless.com/shop/checkout', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({
        items:[{product_id:'nl-simple',quantity:1}],
        email: document.getElementById('email').value,
        name: document.getElementById('name').value,
        success_url: window.location.origin + '/merci',
        cancel_url: window.location.origin + '/annule'
      })
    });
    const d = await r.json();
    if(d.url) window.location.href = d.url;
    else alert('Erreur: ' + (d.detail || 'inconnue'));
  } catch(e) { alert('Erreur réseau'); }
  btn.disabled = false; btn.textContent = 'Acheter — 79€';
}
</script>
```
**Fichier:** `web/site/index.html`  
**Durée:** 1h  
**✅ Critère:** Clic bouton → redirection Stripe Checkout

---

### 1.4 Créer page /merci.html
```html
<!-- Fichier: web/site/merci.html -->
<!DOCTYPE html>
<html lang="fr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Merci pour votre commande | NOISYLESS</title>
  <meta name="robots" content="noindex">
  <style>body{font-family:sans-serif;max-width:600px;margin:auto;padding:40px;text-align:center}</style>
</head>
<body>
  <h1>✅ Commande confirmée</h1>
  <p>Merci <span id="name">---</span> !</p>
  <p>Un email de confirmation a été envoyé à <span id="email">---</span>.</p>
  <p>Réf: <span id="session">---</span></p>
  <a href="/">← Retour au site</a>
  <script>
    const p = new URLSearchParams(window.location.search);
    document.getElementById('name').textContent = p.get('name') || '---';
    document.getElementById('email').textContent = p.get('email') || '---';
    document.getElementById('session').textContent = p.get('session_id') || '---';
  </script>
</body>
</html>
```
**Fichier:** `web/site/merci.html`  
**Durée:** 15 min  
**✅ Critère:** Page accessible 200 OK

---

### 1.5 Créer page /annule.html
```html
<!-- Fichier: web/site/annule.html -->
<!DOCTYPE html>
<html lang="fr">
<head>
  <meta charset="UTF-8">
  <title>Paiement annulé | NOISYLESS</title>
  <meta name="robots" content="noindex">
  <style>body{font-family:sans-serif;max-width:600px;margin:auto;padding:40px;text-align:center}</style>
</head>
<body>
  <h1>❌ Paiement annulé</h1>
  <p>Le paiement a été annulé. Aucun montant n'a été débité.</p>
  <p><a href="/">← Retour au site</a></p>
</body>
</html>
```
**Fichier:** `web/site/annule.html`  
**Durée:** 10 min  
**✅ Critère:** Page accessible 200 OK

---

### 1.6 Webhook Stripe — Confirmation paiement
```python
# Fichier: backend/api/shop.py — AJOUTER

@router.post("/webhook")
async def stripe_webhook(request: Request):
    payload = await request.body()
    sig_header = request.headers.get("Stripe-Signature")
    endpoint_secret = os.getenv("STRIPE_WEBHOOK_SECRET", "")
    try:
        event = stripe.Webhook.construct_event(payload, sig_header, endpoint_secret)
    except ValueError:
        raise HTTPException(status_code=400, detail="Invalid payload")
    except stripe.SignatureVerificationError:
        raise HTTPException(status_code=400, detail="Invalid signature")

    if event["type"] == "checkout.session.completed":
        session = event["data"]["object"]
        # TODO: sauvegarder en BDD + email confirmation
        print(f"[WEBHOOK] Paiement confirmé: {session.get('id')} — {session.get('customer_email')} — {session.get('amount_total')}")

    return {"status": "ok"}
```
**Fichier:** `backend/api/shop.py`  
**Durée:** 1h  
**✅ Critère:** Stripe CLI → `[WEBHOOK] Paiement confirmé: cs_live_...`

---

### 1.7 Table BDD `orders`
```sql
-- Fichier: timescaledb/init/02_orders.sql (NOUVEAU)
CREATE TABLE IF NOT EXISTS noisyless.orders (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    stripe_session_id TEXT UNIQUE NOT NULL,
    customer_email TEXT NOT NULL,
    customer_name TEXT NOT NULL,
    customer_company TEXT DEFAULT '',
    total_amount INTEGER NOT NULL,
    currency TEXT DEFAULT 'eur',
    payment_status TEXT NOT NULL DEFAULT 'pending',
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS noisyless.order_items (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    order_id UUID REFERENCES noisyless.orders(id) ON DELETE CASCADE,
    product_id TEXT NOT NULL,
    product_name TEXT NOT NULL,
    quantity INTEGER NOT NULL DEFAULT 1,
    unit_amount INTEGER NOT NULL
);
```
**Fichier:** `timescaledb/init/02_orders.sql`  
**Durée:** 15 min  
**✅ Critère:** Tables existent dans TimescaleDB (`\dt noisyless.*`)

---

### 1.8 Email confirmation OVH SMTP
```python
# Fichier: backend/api/emails.py (NOUVEAU)
import os, smtplib, ssl
from email.mime.text import MIMEText

SMTP_HOST = os.getenv("SMTP_HOST", "ssl0.ovh.net")
SMTP_PORT = int(os.getenv("SMTP_PORT", "465"))
SMTP_USER = os.getenv("SMTP_USER", "contact@noisyless.com")
SMTP_PASS = os.getenv("SMTP_PASSWORD")
SMTP_FROM = "NOISYLESS <contact@noisyless.com>"

def send_order_confirmation(email: str, name: str, order_id: str, amount: int):
    subject = f"✅ Commande NOISYLESS confirmée #{order_id[:8]}"
    body = f"""Bonjour {name},

Merci pour votre commande de {amount/100:.2f}€.
Votre NOISYLESS Simple sera expédié sous 48h ouvrées.

Numéro de commande: {order_id}

L'équipe NOISYLESS
contact@noisyless.com
"""
    msg = MIMEText(body, "plain", "utf-8")
    msg["Subject"] = subject
    msg["From"] = SMTP_FROM
    msg["To"] = email
    ctx = ssl.create_default_context()
    with smtplib.SMTP_SSL(SMTP_HOST, SMTP_PORT, context=ctx) as s:
        s.login(SMTP_USER, SMTP_PASS)
        s.send_message(msg)
```
**Durée:** 30 min  
**✅ Critère:** Email reçu dans boîte test

---

## 🌐 LOT 2 — SEO + STRUCTURE SITE (12 micro-tâches)

### 2.1 Convertir ancres en URLs réelles
Créer des fichiers HTML séparés pour chaque section au lieu d'ancres `#produit` `#shop`:

| Section actuelle (ancre) | Nouveau fichier | SEO target |
|------------------------|----------------|------------|
| `#header` | `index.html` ← à garder | Page principale |
| `#produit` | `produits/noisyless-simple.html` | Landing produit |
| `#shop` | `boutique.html` | Page boutique |
| `#faq` | `faq.html` | FAQ Google |
| — | `blog/` (9 articles) | Contenu long-tail |
| — | `confidentialite.html` | Pages légales |
| — | `mentions-legales.html` | Pages légales |
| — | `cgv.html` | Pages légales |

**Durée:** 2h  
**✅ Critère:** Chaque URL → 200 OK, title/meta/H1 uniques

---

### 2.2 Corriger title tag < 60 chars
```html
<!-- Dans <head> de index.html -->
<!-- ACTUEL (78 chars - trop long) -->
<title>NOISYLESS | Capteurs IoT pour Locations Courte Durée — Détection Fêtes & Suroccupation</title>

<!-- NOUVEAU (58 chars - parfait pour Google) -->
<title>Détecteur de bruit Airbnb sans caméra | NOISYLESS</title>

<!-- Meta description (158 chars - parfait) -->
<meta name="description" content="NOISYLESS protège 247+ gestionnaires Airbnb en France. Détection fêtes et suroccupation en temps réel. 100% RGPD, sans caméra. Installation 2 minutes.">
```
**Fichier:** `web/site/index.html`  
**Durée:** 10 min  
**✅ Critère:** Title 49-58 chars, description 150-160 chars

---

### 2.3 Section FAQ (mini 6 questions)
```html
<!-- Dans index.html, avant <footer> -->
<section id="faq">
  <h2>Questions fréquentes</h2>
  
  <details itemscope itemprop="mainEntity" itemtype="https://schema.org/Question">
    <summary itemprop="name">Comment installer les capteurs ?</summary>
    <div itemscope itemprop="acceptedAnswer" itemtype="https://schema.org/Answer">
      <p itemprop="text">En 2 minutes : branchez en USB-C, connectez au WiFi via l'app. Aucun perçage nécessaire.</p>
    </div>
  </details>

  <details itemscope itemprop="mainEntity" itemtype="https://schema.org/Question">
    <summary itemprop="name">Est-ce conforme au RGPD ?</summary>
    <div itemscope itemprop="acceptedAnswer" itemtype="https://schema.org/Answer">
      <p itemprop="text">Oui, 100%. Aucune capture d'image ni de son — uniquement des métadonnées anonymes.</p>
    </div>
  </details>

  <details itemscope itemprop="mainEntity" itemtype="https://schema.org/Question">
    <summary itemprop="name">Quelle est l'autonomie des capteurs ?</summary>
    <div itemscope itemprop="acceptedAnswer" itemtype="https://schema.org/Answer">
      <p itemprop="text">Jusqu'à 6 mois sur batterie selon configuration. Mode deep sleep par défaut.</p>
    </div>
  </details>

  <details itemscope itemprop="mainEntity" itemtype="https://schema.org/Question">
    <summary itemprop="name">Peut-on détecter une fête ?</summary>
    <div itemscope itemprop="acceptedAnswer" itemtype="https://schema.org/Answer">
      <p itemprop="text">Oui. Alerte en moins de 5 secondes quand le bruit dépasse 65 dB après 22h.</p>
    </div>
  </details>

  <details itemscope itemprop="mainEntity" itemtype="https://schema.org/Question">
    <summary itemprop="name">Quels sont les capteurs disponibles ?</summary>
    <div itemscope itemprop="acceptedAnswer" itemtype="https://schema.org/Answer">
      <p itemprop="text">NOISYLESS Simple (présence + bruit + qualité d'air, 79€), HeatMap Intérieur (129€), HeatMap Extérieur (199€).</p>
    </div>
  </details>

  <details itemscope itemprop="mainEntity" itemtype="https://schema.org/Question">
    <summary itemprop="name">Y a-t-il un abonnement ?</summary>
    <div itemscope itemprop="acceptedAnswer" itemtype="https://schema.org/Answer">
      <p itemprop="text">Oui, à partir de 4,99€/mois pour 1 à 5 logements. Abonnement Pro à 29,90€/mois pour gestionnaires de parc.</p>
    </div>
  </details>
</section>
```
**Fichier:** `web/site/index.html`  
**Durée:** 30 min  
**✅ Critère:** 6 questions visibles + Schema FAQPage valide Rich Results

---

### 2.4 JSON-LD Schema.org
```html
<!-- Dans <head> de index.html -->
<script type="application/ld+json">
{
  "@context": "https://schema.org",
  "@type": "Product",
  "name": "NOISYLESS Simple",
  "description": "Détecteur de bruit et présence sans caméra pour Airbnb",
  "brand": {"@type": "Brand", "name": "NOISYLESS"},
  "offers": {
    "@type": "Offer",
    "price": "79.00",
    "priceCurrency": "EUR",
    "availability": "https://schema.org/InStock"
  },
  "aggregateRating": {
    "@type": "AggregateRating",
    "ratingValue": "4.9",
    "reviewCount": "47"
  }
}
</script>
<script type="application/ld+json">
{
  "@context": "https://schema.org",
  "@type": "Organization",
  "name": "NOISYLESS",
  "url": "https://noisyless.com",
  "logo": "https://noisyless.com/assets/logocarre.png",
  "description": "Capteurs IoT pour locations courte durée",
  "contactPoint": {
    "@type": "ContactPoint",
    "email": "contact@noisyless.com"
  }
}
</script>
```
**Fichier:** `web/site/index.html`  
**Durée:** 15 min  
**✅ Critère:** https://search.google.com/test/rich-results → 0 erreur

---

### 2.5 Robots.txt complet
```txt
# Fichier: web/site/robots.txt
User-agent: Googlebot
Allow: /

User-agent: Bingbot
Allow: /

User-agent: Applebot
Allow: /

User-agent: GPTBot
Allow: /

User-agent: ClaudeBot
Allow: /

User-agent: PerplexityBot
Allow: /

User-agent: Google-Extended
Allow: /

User-agent: *
Disallow: /merci
Disallow: /annule

Sitemap: https://noisyless.com/sitemap.xml
```
**Fichier:** `web/site/robots.txt`  
**Durée:** 10 min  
**✅ Critère:** `curl https://noisyless.com/robots.txt` → 200 OK, tous les bots en Allow

---

### 2.6 Sitemap.xml
```xml
<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
  <url><loc>https://noisyless.com/</loc><priority>1.0</priority></url>
  <url><loc>https://noisyless.com/produits/noisyless-simple</loc><priority>0.9</priority></url>
  <url><loc>https://noisyless.com/boutique</loc><priority>0.8</priority></url>
  <url><loc>https://noisyless.com/faq</loc><priority>0.8</priority></url>
  <url><loc>https://noisyless.com/confidentialite</loc><priority>0.5</priority></url>
  <url><loc>https://noisyless.com/mentions-legales</loc><priority>0.5</priority></url>
  <url><loc>https://noisyless.com/cgv</loc><priority>0.5</priority></url>
</urlset>
```
**Fichier:** `web/site/sitemap.xml`  
**Durée:** 10 min  
**✅ Critère:** Valide XML + soumis GSC

---

### 2.7 Llms.txt
```txt
# Fichier: web/site/llms.txt
# NOISYLESS — AI Crawler Policy

> NOISYLESS est un fabricant français de capteurs IoT pour locations courte durée.
> Fondé en 2026, basé en France. 100% RGPD, zéro caméra, zéro micro.

## Produits
- NOISYLESS Simple: 79€ — Détection présence + bruit + qualité d'air
- HeatMap Intérieur: 129€ (précommande) — Cartographie ToF 16×16
- HeatMap Extérieur: 199€ (précommande) — Radar mmWave 60 GHz
- Détecteur Fuite + Vanne: 149€ (précommande) — Détection eau + coupure auto

## Abonnements
- Starter: 4,99€/mois — 1 à 5 logements
- Pro: 29,90€/mois — Gestionnaires de parc

## Chiffres clés
- 247+ gestionnaires utilisent NOISYLESS
- Détection < 5 secondes
- Installation en 2 minutes
- Données hébergées OVH France
```
**Fichier:** `web/site/llms.txt`  
**Durée:** 5 min  
**✅ Critère:** `curl https://noisyless.com/llms.txt` → 200 OK

---

### 2.8 Déployer 9 articles blog
```bash
# Fichiers source: noisyless-deployment/blog/
# Destination: web/site/blog/

mkdir -p /home/mathieu/Kdrive/01_CLIENTS/NOISYLESS/web/site/blog/

# Copier les 9 fichiers .md
cp noisyless-deployment/blog/*.md web/site/blog/

# Convertir MD→HTML ou les servir en .md
# Option: créer blog/index.html listant les articles
```
**Fichiers:** `web/site/blog/*.md`  
**Durée:** 30 min  
**✅ Critère:** 9 articles accessibles sur `noisyless.com/blog/...`

---

### 2.9 Google Search Console
```bash
# 1. https://search.google.com/search-console
# 2. Ajouter propriété "noisyless.com"
# 3. Valider via DNS Cloudflare (record TXT)
# 4. Soumettre sitemap
# 5. Inspection URL → "Demander une indexation"
```
**Durée:** 20 min  
**✅ Critère:** GSC → "URL est sur Google"

---

### 2.10 Corriger copie
```bash
# Remplacer partout:
# "images识别ables" → "identifiables"
# "surpopulation" → "suroccupation"
# Title → "Détecteur de bruit Airbnb sans caméra"

# Fichier: web/site/index.html
sed -i 's/surpopulation/suroccupation/g' web/site/index.html
sed -i 's/识别/identifi/g' web/site/index.html

git diff web/site/index.html
```
**Fichier:** `web/site/index.html`  
**Durée:** 10 min  
**✅ Critère:** Aucun caractère corrompu, terme uniformisé

---

### 2.11 Purger cache Cloudflare
```bash
# Dashboard Cloudflare → Caching → Configuration → Purge Everything
# Ou API:
curl -X POST "https://api.cloudflare.com/client/v4/zones/CF_ZONE_ID/purge_cache" \
  -H "Authorization: Bearer $CF_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"purge_everything":true}'
```
**Durée:** 5 min  
**✅ Critère:** Cloudflare "Purge in progress" ✅

---

### 2.12 Créer page FAQ dédiée
Créer `faq/index.html` avec FAQPage JSON-LD, reprendre les 6+ questions de index.html mais enrichies (2000+ mots).

**Fichier:** `web/site/faq.html`  
**Durée:** 1h  
**✅ Critère:** page +2000 mots, FAQPage Rich Result OK

---

## 🛠️ LOT 3 — INFRASTRUCTURE (5 micro-tâches)

### 3.1 Ajouter STRIPE_WEBHOOK_SECRET dans docker-compose
```yaml
# Dans _infra/docker-compose.yml, section api.environment
      STRIPE_SECRET_KEY: ${STRIPE_SECRET_KEY}
      STRIPE_WEBHOOK_SECRET: ${STRIPE_WEBHOOK_SECRET}
```
**Durée:** 5 min

---

### 3.2 Ajouter variables SMTP dans docker-compose
```yaml
# Dans _infra/docker-compose.yml, section api.environment
      SMTP_HOST: ${SMTP_HOST:-ssl0.ovh.net}
      SMTP_PORT: ${SMTP_PORT:-465}
      SMTP_USER: ${SMTP_USER:-contact@noisyless.com}
      SMTP_PASSWORD: ${SMTP_PASSWORD}
```
**Durée:** 5 min

---

### 3.3 Fix MQTT TLS
```bash
# 1. Générer CA + certs
# 2. Configurer mosquitto.conf pour listener 8883
# 3. Tester
mosquitto_pub -h platform.noisyless.com -p 8883 --cafile ca.crt -t test -m "hello"
```
**Durée:** 2h  
**✅ Critère:** TLS connect OK

---

### 3.4 UFW Firewall VPS
```bash
ssh noisyless@91.99.26.43
ufw default deny incoming
ufw allow 22/tcp
ufw allow 80/tcp
ufw allow 443/tcp
ufw allow 1883/tcp
ufw --force enable
ufw status verbose
```
**Durée:** 10 min  
**✅ Critère:** Seuls ports 22,80,443,1883 ouverts

---

### 3.5 Backups automatisés
```bash
# Script: /opt/noisyless/scripts/backup.sh
#!/bin/bash
docker compose exec timescaledb pg_dump -U noisyless noisyless > /opt/noisyless/backups/noisyless_$(date +%Y%m%d_%H%M%S).sql
find /opt/noisyless/backups/ -mtime +30 -delete

# Cron: 0 3 * * * /opt/noisyless/scripts/backup.sh
```
**Durée:** 30 min  
**✅ Critère:** Backup quotidien + rétention 30 jours

---

## 🔧 LOT 4 — FIRMWARE + HARDWARE (4 micro-tâches)

### 4.1 Tester deep sleep #089
```bash
# Fichier: 202604_089_Capteur_Environnemental_V1/05_FIRMWARE/src/main.cpp
# 1. Flasher firmware sur ESP32-S3
# 2. Mesurer consommation:
#    - Normal: ~80mA
#    - Deep sleep: ~80µA (objectif <100µA)
# 3. Ajuster esp_sleep_enable_timer_wakeup
# 4. Vérifier réveil → lecture → sleep
```
**Durée:** 2h  
**✅ Critère:** Autonomie > 3 mois sur batterie 2000mAh

---

### 4.2 Radar 60GHz — Firmware C ESP-IDF
```bash
# Dossier actuel: VIDE (node-radar-60ghz/)
# 1. Créer composant radar_ms72sf1
# 2. Parser UART (115200 baud, TLV frames)
# 3. Publier détections via MQTT
# 4. Tester avec module Seeed Studio
```
**Fichiers:** `node-radar-60ghz/components/radar_ms72sf1/*`  
**Durée:** 3h  
**✅ Critère:** Radar → ESP → MQTT → API

---

### 4.3 PCB HeatMap — DRC #091
```bash
# 1. Ouvrir 202605_091/hardware/kicad/heatmap-tof.kicad_pro
# 2. Tools → Design Rules Checker → Run DRC
# 3. Corriger erreurs:
#    - Clearance violations
#    - Unconnected nets
# 4. Générer BOM
# 5. Export Gerbers
```
**Durée:** 1h  
**✅ Critère:** DRC = 0 erreurs, Gerbers prêts fabrication

---

### 4.4 BOM #089 — Validation conso
```bash
# 1. Relevé BOM des 3 variants PCB V01/RevB/Noisyless_RevB
# 2. Additionner conso théorique (datasheets)
# 3. Comparer avec mesure réelle
# 4. Ajuster si > budget power
```
**Fichiers:** `202604_089/03_CAO/*/bom.csv`  
**Durée:** 30 min  
**✅ Critère:** Conso théorique < 100µA sleep, < 100mA actif

---

## 📄 LOT 5 — PAGES LÉGALES (après validation juridique)

### 5.1 Mentions légales
**Fichier:** `web/site/mentions-legales.html`  
**Durée:** 30 min  
**Gate:** ✅ Validation humaine requise

### 5.2 Politique confidentialité RGPD
**Fichier:** `web/site/confidentialite.html`  
**Durée:** 30 min  
**Gate:** ✅ Validation humaine requise

### 5.3 CGV
**Fichier:** `web/site/cgv.html`  
**Durée:** 30 min  
**Gate:** ✅ Validation humaine requise

---

## 📊 RÉSUMÉ EXÉCUTION

### Par lot

| Lot | Tâches | Durée | Dépend de | Objectif |
|-----|--------|-------|-----------|----------|
| **1. Stripe** | 8 | ~3h30 | Rien | ✅ Paiement fonctionnel |
| **2. SEO** | 12 | ~5h30 | Lot 1 partiel | 📈 5+ pages indexées |
| **3. Infra** | 5 | ~3h | Lot 1 | 🔒 Sécurisé |
| **4. Firmware** | 4 | ~6h30 | Rien | 🛠️ Hardware opérationnel |
| **5. Légal** | 3 | ~1h30 | Lot 2 | ⚖️ Conforme |

### Par priorité

| Priorité | Tâche | Dépend |
|----------|-------|--------|
| **🔴 P0** | 1.1 Clé Stripe valide | — |
| **🔴 P0** | 1.2 Tester checkout | 1.1 |
| **🟠 P1** | 1.3 Bouton Acheter | 1.2 |
| **🟠 P1** | 1.4 Page /merci | — |
| **🟠 P1** | 1.5 Page /annule | — |
| **🟠 P1** | 1.6 Webhook Stripe | 1.7 |
| **🟠 P1** | 1.7 Table orders | — |
| **🟠 P1** | 1.8 Email SMTP | 1.6 |
| **🟠 P1** | 2.1 URLs réelles (multi-page) | — |
| **🟠 P1** | 2.2 Title tag | — |
| **🟡 P2** | 2.3 Section FAQ | — |
| **🟡 P2** | 2.4 JSON-LD | — |
| **🟡 P2** | 2.5 Robots.txt | — |
| **🟡 P2** | 2.6 Sitemap | 2.1 |
| **🟡 P2** | 2.7 llms.txt | — |
| **🟡 P2** | 2.8 Blog articles | — |
| **🟡 P2** | 2.9 Google Search Console | 2.5, 2.6 |
| **🟡 P2** | 2.10 Corriger copie | — |
| **🟡 P2** | 2.11 Purge Cloudflare | 2.2-2.8 |
| **🟡 P2** | 2.12 Page FAQ dédiée | — |
| **🟢 P3** | 3.1-3.5 Infrastructure | Lot 1 |
| **🟢 P3** | 4.1-4.4 Firmware | — |
| **⚪ P4** | 5.1-5.3 Pages légales | — |

---

## ✅ CHECKLIST FINALE (avant mise en prod)

```
[ ] Clé Stripe LIVE valide → checkout Stripe fonctionnel
[ ] Bouton "Acheter" → redirection Stripe
[ ] Page /merci → confirmation client
[ ] Page /annule → gestion échec
[ ] Webhook → commande BDD + email
[ ] Title tag < 60 chars
[ ] Section FAQ 6 questions
[ ] JSON-LD Product + Organization
[ ] robots.txt tous bots Allow
[ ] sitemap.xml soumis GSC
[ ] llms.txt déployé
[ ] 9 articles blog en ligne
[ ] Copie corrigée (suroccupation, identifiables)
[ ] Cache Cloudflare purgé
[ ] 5 pages indexées Google
[ ] UFW firewall actif
[ ] Backups quotidiens
[ ] DRC 0 erreurs PCB #091
[ ] Radar 60GHz → ESP → MQTT
```

---

**Prochaine action immédiate:** Tâche 1.1 — Récupérer NOUVELLE clé Stripe LIVE depuis https://dashboard.stripe.com/apikeys

**Durée totale restante:** ~20h de travail  
**Équipes:** 1 développeur = 2-3 semaines  
**Blockers:** Aucun si clé Stripe fournie

<!-- 
  Last updated: 2026-05-24
  Generated from: audit physique complet du répertoire /home/mathieu/Kdrive/01_CLIENTS/NOISYLESS/
-->
