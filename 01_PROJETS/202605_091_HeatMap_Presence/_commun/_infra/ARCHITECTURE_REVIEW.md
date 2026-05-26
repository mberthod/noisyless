# NOISYLESS — Architecture Review

## ✅ Ce qui est excellent

### 1. Philosophie solide

- **"Un backend, des modules hétérogènes"** : La bonne approche. Le champ `model` comme clé de routage de bout en bout évite la duplication de code.
- **Sécurité native** : Tu as identifié la dette technique d'EkoFlush et tu la préviens. C'est la maturité d'un projet indus.
- **Déploiement reproductible** : `docker-compose` + secrets générés + restore < 20 min = professionnel.

### 2. Firmware : bonne séparation

```
noisyless-core (commun) + HAL capteur (par model)
```

C'est l'architecture propre :
- 80% du code partagé
- 20% spécifique par type
- Build matrix = binaire léger par SKU

### 3. Schéma de télémétrie polymorphe

```json
{
  "schema": "nl.telemetry.v1",
  "model":  "nl-map-in",
  "duid":   "24:EC:4A:23:36:BC",
  "metrics": { ... typé selon model ... }
}
```

✅ Versionné + enveloppe commune + `metrics` polymorphe = extensible sans breaking changes.

### 4. Correction des dettes EkoFlush

| Dette | Correction |
|-------|------------|
| MQTT plain | TLS 8883 |
| Creds en clair | NVS chiffré |
| OTA non signée | Ed25519 + A/B |
| Compte MQTT partagé | Username = DUID |
| `users.json` | Postgres |

Tout est là. C'est du travail sérieux.

### 5. OTA industrielle

- Signée Ed25519 ✅
- A/B + rollback auto ✅
- Canaux `stable`/`beta` ✅
- `min_version` pour forçage sécurité ✅

C'est le niveau requis pour ne pas bricker 200 modules.

---

## ⚠️ Points d'attention / Incohérences

### 1. **Mosquitto vs EMQX — Tu hésites**

> *"EMQX (cible indus) plutôt que Mosquitto... Mosquitto reste acceptable pour démarrer"*

**Problème :** Tu as **déjà Mosquitto en prod** dans ton docker-compose actuel. Changer maintenant = migration douloureuse.

**Recommandation :**

```
Phase 1 (maintenant) : Mosquitto + auth ACL
Phase 2 (100+ modules) : Migration EMQX clusterisé
```

Mosquitto gère très bien :
- TLS 8883
- Auth par username/password
- ACL par topic
- Jusqu'à ~10k clients concurrents

**Décision :** Garde Mosquitto pour le Bloc 1. Migre si/qua nd tu scales.

---

### 2. **Continuous Aggregates TimescaleDB — Sous-utilisé**

Tu mentionnes :
> *"Remplacer le Heatmap Aggregator Python par des continuous aggregates TimescaleDB"*

**Mais ton architecture actuelle a encore `heatmap-agg` en Python !**

Ce service custom est :
- Une source de bugs (merge de grids, timezones, retries)
- Un point de défaillance supplémentaire
- Moins performant que du SQL natif

**Recommandation :**

```sql
-- Hypertable
SELECT create_hypertable('measurements', 'time');

-- Continuous aggregate 5min
CREATE MATERIALIZED VIEW heatmap_agg_5min
WITH (timescaledb.continuous) AS
SELECT 
  time_bucket('5 minutes', time) AS window_start,
  villa_id,
  zone,
  SUM((payload->>'total_count')::int) AS total_count,
  MAX((payload->>'max_count')::int) AS max_count,
  jsonb_agg(payload->'grid') AS grids
FROM measurements
WHERE model = 'nl-map-in'
GROUP BY window_start, villa_id, zone;
```

**Gain :** -1 service à maintenir, +performance, +fiabilité.

---

### 3. **`alert-engine` — Logique métier dans un service custom**

Même problème. Tu peux faire :

```sql
-- Trigger sur heatmap_agg_5min
CREATE OR REPLACE FUNCTION check_thresholds()
RETURNS TRIGGER AS $$
BEGIN
  IF NEW.max_count > (SELECT threshold_count FROM alert_thresholds 
                      WHERE villa_id = NEW.villa_id) THEN
    -- Insert dans une table alert_pending
    -- Déclencheur secondaire envoie Telegram
  END IF;
  RETURN NEW;
END;
$$ LANGUAGE plpgsql;
```

**Ou** garde le service Python mais :
- Abonne-toi aux **aggregates**, pas aux mesures brutes
- Réduis la fréquence de 30s → 5min (inutile de poller plus vite que la fenêtre)

---

### 4. **Architecture actuelle vs Cible — Écart non documenté**

Tu as deux documents :
1. `ARCHITECTURE.md` (actuel : Mosquitto, services Python, pas de TLS MQTT)
2. `ARCHITECTURE_CIBLE.md` (ce document : EMQX, TLS, continuous aggregates)

**Manque :** Un document `MIGRATION.md` qui dit :

| Composant | État actuel | Cible | Quand | Effort |
|-----------|-------------|-------|-------|--------|
| Mosquitto | Plain 1883 | TLS 8883 | Bloc 1 | 2h |
| mqtt-ingest | Python | Python + routage model | Bloc 1 | 4h |
| heatmap-agg | Python | Timescale continuous | Bloc 1 | 6h |
| alert-engine | Python 30s | Python 5min ou trigger | Bloc 1 | 2h |
| Secrets | `.env` clair | Généré + hors-VPS | Bloc 1 | 1h |
| OTA | Manuelle | Signée + A/B | Bloc 2 | 16h |

---

### 5. **Trous de sécurité restants**

| Couche | État | Cible |
|--------|------|-------|
| MQTT TLS | ✗ (actuel) | ✓ (Bloc 1) |
| MQTT auth par module | ✗ | ✓ (Bloc 1) |
| NVS chiffré firmware | ? | ✓ (Bloc 1) |
| Secure Boot | ? | ✓ (Bloc 2) |
| JWT secret généré | ✗ (devinable) | ✓ (Bloc 1) |
| 2FA admin | ✗ | Moyen terme |

**Priorité Bloc 1 :**
1. Générer `JWT_SECRET` et `DB_PASSWORD` aléatoires
2. MQTT auth + ACL (Mosquitto le permet sans EMQX)
3. TLS MQTT (certificat serveur + client optionnel)

---

### 6. **Modèle économique — Flou**

> *"Modèle d'abonnement : par module / par logement / par compte — impacte le gating §5 et la facturation"*

**Ce n'est pas qu'une question de facturation.** C'est :

| Modèle | Impact technique |
|--------|------------------|
| Par module | Gating par `duid` dans API |
| Par logement | Gating par `villa_id` + multi-modules |
| Par compte | Gating par `user_id` + quota global |

**Recommandation :** Commence **par compte** (plus simple) :
- 1 compte = 1 villa = N modules
- Upgrade vers **par module** si tu vends du multi-villas

---

### 7. **Cloudflare Tunnel — Redondant avec SSL direct**

Tu as :
- SSL Let's Encrypt DNS-01 ✅
- `cloudflared` tunnel ✅

**Problème :** Tu paies Cloudflare **et** tu gères des certificats locaux.

**Deux options :**

1. **Cloudflare Tunnel uniquement** (recommandé) :
   - Pas de ports ouverts sur le VPS
   - Cloudflare gère SSL
   - Certificat local inutile

2. **SSL direct uniquement** (actuel) :
   - Garde Let's Encrypt
   - Retire `cloudflared`
   - Ouvre 80/443 sur le firewall

**Décision :** Tunnel = plus sécurisé (pas de ports exposés). Garde-le, retire Let's Encrypt local.

---

## 📋 Checklist Bloc 1 (priorisée)

### Semaine 1 — Sécurité de base

- [ ] Générer `JWT_SECRET` et `DB_PASSWORD` aléatoires (32+ bytes)
- [ ] Stocker hors-VPS (password manager) + backup chiffré
- [ ] MQTT auth : username = DUID, password = hash(device_secret)
- [ ] MQTT ACL : `noisyless/<duid>/#` uniquement
- [ ] MQTT TLS : certificat serveur + port 8883

### Semaine 2 — Firmware + OTA

- [ ] NVS chiffré (ESP-IDF `nvs_flash_encrypt`)
- [ ] OTA signée Ed25519 (clé privée hors CI)
- [ ] A/B partition + rollback auto
- [ ] Endpoint `/v1/ota/check` par model/canal

### Semaine 3 — Simplification backend

- [ ] Continuous aggregates TimescaleDB (remplacer `heatmap-agg`)
- [ ] `alert-engine` → subscribe aggregates, pas raw
- [ ] Migration `.env` → secrets générés
- [ ] Backup chiffré age + rotation

### Semaine 4 — Déploiement reproductible

- [ ] Script `bootstrap.sh` (génère secrets, init DB)
- [ ] Script `restore.sh` (< 20 min sur VPS vierge)
- [ ] Documentation deploy + runbook incident

---

## 🎯 Verdict

**Ce document est une excellente base.** Il manque :

1. **Un plan de migration** (actuel → cible, étape par étape)
2. **Des décisions tranchées** (Mosquitto vs EMQX, Tunnel vs SSL direct)
3. **Une priorisation réaliste** (Bloc 1 = vendable, Bloc 2+ = scale)

**Recommandation :**

1. **Fige le Bloc 1** (sécurité + OTA + deployment)
2. **Vends le module simple** (`nl-presence`)
3. **Itère avec les retours clients**
4. **Ajoute `nl-map-in`** (Bloc 2)
5. **Scale** (Bloc 3)

> *"Drive the Tesla"* — Ne construis pas le Bloc 3 avant que le Bloc 1 génère du CA.

---

*Review : 24 Mai 2026*
