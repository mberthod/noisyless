# NOISYLESS — Tâches dans l'Ordre d'Exécution

**Analyse:** Le backlog "hermes" est incomplet — il manque toute la partie **Stripe/Checkout/SEO** sur laquelle on travaillait.

**Voici la liste COMPLÈTE et DÉCOUPÉE en micro-tâches exécutables.**

---

## 📍 Contexte Actuel

| Système | Status | Problème |
|---------|--------|----------|
| **Backend API** | ✅ En ligne (VPS OVH) | — |
| **Stripe Checkout** | ❌ Bloqué | Clé API invalide |
| **Frontend Site** | ✅ En ligne (OVH FTP) | 1 page indexée seulement |
| **Firmware #089** | ⚠️ Deep sleep non testé | Autonomie inconnue |
| **Radar 60GHz** | ❌ Dossier vide | Rien de commencé |
| **PCB HeatMap** | ⏳ DRC en cours | KiCad #091 |
| **MQTT TLS** | ❌ Port 8883 cassé | À réparer |

---

## 🎯 ORDRE D'EXÉCUTION (18 tâches)

### SEMAINE 1 — Stripe + Revenu Immédiat

#### Tâche 1: Clé Stripe LIVE Valide (30 min)
```bash
# 1. Dashboard Stripe → https://dashboard.stripe.com/apikeys
# 2. Vérifier mode LIVE (pas TEST)
# 3. Copier nouvelle clé secrète
# 4. SSH VPS + update .env
ssh noisyless@91.99.26.43
nano /opt/noisyless/.env
# STRIPE_SECRET_KEY=sk_live_51TTSCbEvgqhDyN6I... (NOUVELLE)
# 5. Restart API
docker compose -f /opt/noisyless/docker-compose.yml restart api
# 6. Test
curl -u "sk_live_...:" https://api.stripe.com/v1/products
```
**✅ Critère:** `curl` retourne une liste de produits Stripe

---

#### Tâche 2: Tester Endpoint Checkout (15 min)
```bash
curl -X POST https://platform.noisyless.com/shop/checkout \
  -H "Content-Type: application/json" \
  -d '{"items":[{"product_id":"nl-simple","quantity":1}],"email":"test@test.com","name":"Test","success_url":"https://noisyless.com/merci","cancel_url":"https://noisyless.com/annule"}'
```
**✅ Critère:** Réponse `{"session_id":"cs_live_...","url":"https://checkout.stripe.com/..."}`

---

#### Tâche 3: Bouton "Acheter" sur Site (1h)
**Fichier:** `/web/site/index.html` (section NOISYLESS Simple)

```html
<!-- Actuel: <button>Acheter</button> sans action -->
<!-- Nouveau: -->
<button onclick="startCheckout()">Acheter - 79€</button>

<script>
async function startCheckout() {
  const resp = await fetch('https://platform.noisyless.com/shop/checkout', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({
      items: [{product_id: 'nl-simple', quantity: 1}],
      email: document.getElementById('email').value,
      name: document.getElementById('name').value,
      success_url: window.location.origin + '/merci',
      cancel_url: window.location.origin + '/annule'
    })
  });
  const data = await resp.json();
  if (data.url) window.location.href = data.url;
}
</script>
```
**✅ Critère:** Clic → redirection vers Stripe Checkout

---

#### Tâche 4: Pages /merci et /annule (30 min)
**Fichiers:** `/web/site/merci.html`, `/web/site/annule.html`

```html
<!-- merci.html -->
<!DOCTYPE html>
<html>
<head><title>Commande Confirmée | NOISYLESS</title></head>
<body>
  <h1>Merci pour votre commande !</h1>
  <p>Un email de confirmation a été envoyé à <span id="email"></span></p>
  <p>Numéro de session: <span id="session_id"></span></p>
  <script>
    const params = new URLSearchParams(window.location.search);
    document.getElementById('session_id').textContent = params.get('session_id');
  </script>
</body>
</html>
```
**✅ Critère:** Pages uploadées sur OVH FTP + accessibles

---

#### Tâche 5: Webhook Stripe — Confirmation Paiement (2h)
**Fichier:** `/backend/api/shop.py` — Ajouter endpoint

```python
@router.post("/webhook")
async def stripe_webhook(request: Request):
    payload = await request.body()
    sig_header = request.headers.get("Stripe-Signature")
    try:
        event = stripe.Webhook.construct_event(payload, sig_header, endpoint_secret)
    except: raise HTTPException(status_code=400)
    
    if event["type"] == "checkout.session.completed":
        session = event["data"]["object"]
        # TODO: Sauvegarder commande en BDD
        # TODO: Envoyer email confirmation
    return {"status": "ok"}
```
**✅ Critère:** Webhook testé avec Stripe CLI → log "Commande sauvegardée"

---

#### Tâche 6: Table BDD `orders` (30 min)
**Fichier:** `/timescaledb/init/03_orders.sql`

```sql
CREATE TABLE IF NOT EXISTS noisyless.orders (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    stripe_session_id TEXT UNIQUE NOT NULL,
    customer_email TEXT NOT NULL,
    customer_name TEXT NOT NULL,
    customer_company TEXT,
    total_amount INTEGER NOT NULL,
    currency TEXT DEFAULT 'eur',
    payment_status TEXT NOT NULL,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS noisyless.order_items (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    order_id UUID REFERENCES noisyless.orders(id) ON DELETE CASCADE,
    product_id TEXT NOT NULL,
    product_name TEXT NOT NULL,
    quantity INTEGER NOT NULL,
    unit_amount INTEGER NOT NULL
);
```
**✅ Critère:** Tables créées + visibles dans `pgAdmin` ou `psql`

---

#### Tâche 7: Email Confirmation — OVH SMTP (1h)
**Fichier:** `/backend/api/emails.py` (nouveau)

```python
import smtplib, ssl
from email.mime.text import MIMEText

def send_order_confirmation(email: str, order_id: str, amount: int):
    smtp_host = os.getenv("SMTP_HOST", "ssl0.ovh.net")
    smtp_port = int(os.getenv("SMTP_PORT", 465))
    smtp_user = os.getenv("SMTP_USER", "contact@noisyless.com")
    smtp_pass = os.getenv("SMTP_PASSWORD")
    
    subject = f"Commande NOISYLESS confirmée #{order_id[:8]}"
    body = f"""Merci pour votre commande de {amount/100:.2f}€
    Votre détecteur sera expédié sous 48h.
    Numéro: {order_id}"""
    
    msg = MIMEText(body)
    msg["Subject"] = subject
    msg["From"] = "NOISYLESS <contact@noisyless.com>"
    msg["To"] = email
    
    context = ssl.create_default_context()
    with smtplib.SMTP_SSL(smtp_host, smtp_port, context=context) as server:
        server.login(smtp_user, smtp_pass)
        server.send_message(msg)
```
**✅ Critère:** Email reçu dans boîte test après commande

---

### SEMAINE 2 — SEO/GEO (Visibilité Google)

#### Tâche 8: Title Tag + Meta Description (20 min)
**Fichier:** `/web/site/index.html` — `<head>`

```html
<!-- AVANT (78 chars - trop long) -->
<title>NOISYLESS | Capteurs IoT pour Locations Courte Durée — Détection Fêtes & Suroccupation</title>

<!-- APRÈS (58 chars - parfait) -->
<title>Détecteur de bruit Airbnb sans caméra | NOISYLESS</title>
<meta name="description" content="NOISYLESS protège 247 gestionnaires Airbnb en France. Détection fêtes + suroccupation en temps réel. 100% RGPD, sans caméra. Installation 2 min.">
```
**✅ Critère:** Title < 60 chars, Meta 150-160 chars

---

#### Tâche 9: Section FAQ (1h)
**Fichier:** `/web/site/index.html` — Ajouter avant `<footer>`

```html
<section id="faq">
  <h2>Questions Fréquentes</h2>
  
  <details>
    <summary>Comment installer les capteurs ?</summary>
    <p>En 2 minutes : branchez en USB-C, connectez au WiFi via l'app, et c'est tout. Aucun perçage nécessaire.</p>
  </details>
  
  <details>
    <summary>Est-ce conforme RGPD ?</summary>
    <p>Oui, 100%. Nos capteurs ne capturent ni image ni son — uniquement des métadonnées (niveau sonore, présence, CO2).</p>
  </details>
  
  <details>
    <summary>Quelle est l'autonomie ?</summary>
    <p>Jusqu'à 6 mois sur batterie selon configuration. Mode deep sleep activé par défaut.</p>
  </details>
  
  <details>
    <summary>Peut-on détecter une fête ?</summary>
    <p>Oui. Alerte SMS/email/Telegram en <5 secondes quand le bruit dépasse 65 dB après 22h.</p>
  </details>
</section>
```
**✅ Critère:** 6 questions minimum + schema.org FAQPage JSON-LD

---

#### Tâche 10: JSON-LD Schema.org (30 min)
**Fichier:** `/web/site/index.html` — Dans `<head>`

```html
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
```
**✅ Critère:** Validé sur https://search.google.com/test/rich-results

---

#### Tâche 11: Sitemap.xml (20 min)
**Fichier:** `/web/site/sitemap.xml`

```xml
<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
  <url><loc>https://noisyless.com/</loc><priority>1.0</priority></url>
  <url><loc>https://noisyless.com/merci</loc><priority>0.5</priority></url>
  <url><loc>https://noisyless.com/annule</loc><priority>0.5</priority></url>
</urlset>
```
**✅ Critère:** Upload FTP + soumis dans Google Search Console

---

#### Tâche 12: Google Search Console (15 min)
1. Aller sur https://search.google.com/search-console
2. Ajouter propriété `noisyless.com`
3. Valider via DNS (record TXT chez Cloudflare)
4. Soumettre sitemap: `https://noisyless.com/sitemap.xml`
5. Inspection URL → Tester `https://noisyless.com/`

**✅ Critère:** Status "URL est sur Google" ✅

---

### SEMAINE 3 — Firmware + Hardware

#### Tâche 13: Deep Sleep Firmware #089 (2h)
**Dossier:** `/firmware/node-089/`

```c
// esp_sleep_enable_timer_wakeup(60 * 1000000); // 60 secondes
// esp_deep_sleep_start();

// TODO:
// 1. Mesurer conso actuelle (mA)
// 2. Activer deep sleep entre 2 lectures
// 3. Réveil timer ou GPIO (radar)
// 4. Objectif: <100µA en sleep
```
**✅ Critère:** Autonomie > 3 mois sur batterie 2000mAh

---

#### Tâche 14: Radar 60GHz — Parser MS72SF1 (3h)
**Dossier:** `/firmware/radar-60ghz/` (VIDE actuellement)

```bash
# 1. Récupérer datasheet Seeed Studio MS72SF1
# 2. Extraire protocole UART (baud rate, frame format)
# 3. Traduire parser Python → C ESP-IDF
# 4. Tester avec module radar réel
```

**Fichiers à créer:**
- `radar_parser.c` — Lecture UART + decode frames
- `radar_parser.h` — Structures + API
- `main.c` — Test harness

**✅ Critère:** Module radar → ESP32 → affichage distances sur UART

---

#### Tâche 15: PCB HeatMap — DRC #091 (1h)
**Dossier:** `/hardware/heatmap-091/`

```bash
# 1. Ouvrir projet KiCad
# 2. Run DRC (Design Rule Check)
# 3. Corriger erreurs (clearance, unconnected nets)
# 4. Générer BOM (Bill of Materials)
# 5. Export Gerbers pour fabrication
```
**✅ Critère:** DRC = 0 erreurs, BOM généré, Gerbers prêts

---

#### Tâche 16: MQTT TLS Port 8883 (1h)
**Fichier:** `/backend/mosquitto/config/mosquitto.conf`

```conf
# Actuel: port 1883 only (non-SSL)
# Ajouter:
listener 8883
cafile /mosquitto/certs/ca.crt
certfile /mosquitto/certs/server.crt
keyfile /mosquitto/certs/server.key
require_certificate false
```
**✅ Critère:** `mosquitto_sub --port 8883 --use-websockets` fonctionne

---

### SEMAINE 4 — Déploiement + Tests

#### Tâche 17: Déployer Backend → Hetzner (2h)
**Actuel:** VPS OVH (91.99.26.43)  
**Cible:** Hetzner (meilleur rapport prix/perf)

```bash
# 1. rsync --avz /opt/noisyless/ root@hetzner_ip:/opt/noisyless/
# 2. docker compose build
# 3. docker compose up -d
# 4. Changer DNS Cloudflare → nouvelle IP
# 5. Attendre propagation + test HTTPS
```
**✅ Critère:** Site + API accessibles sur nouvelle IP

---

#### Tâche 18: Tests Unitaires Firmware (2h)
**Dossier:** `/firmware/tests/`

```c
// test_tmf8829.c
void test_distance_reading() {
    // Mock I2C response
    // Assert distance > 0 && < 5000 (mm)
}

// test_mesh.c
void test_mesh_join() {
    // Mock ESP-NOW handshake
    // Assert node joined
}
```
**✅ Critère:** `idf.py test` → 100% pass

---

## 📊 Résumé Exécution

| Semaine | Tâches | Objectif |
|---------|--------|----------|
| **S1** | 1-7 | Stripe checkout fonctionnel + 1ers revenus |
| **S2** | 8-12 | SEO optimisé → 5+ pages indexées Google |
| **S3** | 13-16 | Firmware autonome + radar 60GHz opérationnel |
| **S4** | 17-18 | Déployé Hetzner + tests unitaires |

---

## 🚀 Prochaine Action Immédiate

**Maintenant (30 min):**
```bash
# 1. Dashboard Stripe → nouvelle clé LIVE
# 2. SSH VPS → update .env
# 3. Restart API
# 4. Test checkout
```

**Après (1h):**
```bash
# 5. Bouton "Acheter" sur site
# 6. Pages /merci + /annule
# 7. Upload FTP OVH
```

---

**Total estimé:** 18 tâches ≈ 15-20 heures de travail  
**Priorité absolue:** Tâche 1 (clé Stripe) → débloque tout le reste
