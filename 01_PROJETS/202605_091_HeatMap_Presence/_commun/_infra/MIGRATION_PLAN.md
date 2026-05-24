# NOISYLESS — Plan de migration (actuel → cible)

État : mai 2026. Complète `ARCHITECTURE_CIBLE.md`. Trace l'écart entre l'archi actuelle (`ARCHITECTURE.md` : Mosquitto plain, services Python, secrets devinables, pas de claim) et la cible, avec les décisions tranchées et les pièges à ne pas coder de travers.

---

## 0. Décisions tranchées

Clôture les hésitations du doc cible et les points de la review.

| Sujet | Décision | Raison |
|---|---|---|
| Broker | **Mosquitto** reste pour Bloc 1. EMQX repoussé au Bloc 3 (si >qq centaines de clients concurrents) | Pas de migration douloureuse maintenant ; Mosquitto gère TLS + auth + ACL sans problème à cette échelle |
| Mécanisme d'auth MQTT | `password_file` + `acl_file` **régénérés par le backend** (pattern `auth.js` EkoFlush) | Conséquence directe de garder Mosquitto : pas de hook HTTP temps réel. Le gating abonnement = régénération d'ACL sur changement d'état (event/cron), pas en direct |
| Exposition réseau | Dashboard/API/WSS via **tunnel cloudflared** ; **MQTT/TLS 8883 exposé en TCP direct** (firewall ufw) | Le tunnel Cloudflare ne transporte pas le MQTT TCP brut (Spectrum = payant). Le flux device se sécurise par TLS + auth/module + firewall, **pas** par le tunnel |
| TLS public HTTP | **Tunnel uniquement**, Let's Encrypt local retiré | Évite la redondance cert local + Cloudflare. Ne concerne **que** le HTTP/WSS |
| Modèle éco | **Par module** (gating par `duid`) | Marché LCD multi-biens : « par compte » laisse l'argent du multi-villas sur la table et force une migration de billing plus tard. Coût technique « par module » = un flag par device |
| Agrégation scalaire | **Continuous aggregates** TimescaleDB | Présence, bruit, air, `max_count`, `total_count` : SQL natif, -1 service Python |
| Agrégation heatmap (grille 2D) | **Fonction d'agrégation d'arrays** (grille en `int[]`, fusion cellule par cellule), pas `jsonb_agg` | `jsonb_agg` empile les grilles, ne les fusionne pas. Pour du 8×8 fixe, une agrégation d'array reste native Timescale |
| Alerting | **Worker Python** qui lit les agrégats toutes les 5 min | Pas de trigger PLpgSQL → Telegram : I/O réseau en transaction = fragile, pas de retry, couple la DB à Telegram |
| Secure Boot v2 | **Après** l'OTA signée validée en réel (fin Bloc 1 / Bloc 2) | Activation irréversible : avant une chaîne OTA signée fonctionnelle, risque de verrouiller le parc |

---

## 1. Tableau de migration

Efforts = ordres de grandeur. ⚠ = chantier à risque/sous-estimé.

| Composant | État actuel | Cible | Bloc | Dépend de | Effort |
|---|---|---|---|---|---|
| Secrets serveur | `.env` clair, devinable | générés 32 B + hors-VPS chiffré | 1 | — | 1 h |
| Auth MQTT | aucune (anonyme) | `username=DUID` + ACL `noisyless/<duid>/#` | 1 | secret device | 4 h |
| TLS MQTT | plain 1883 | TLS 8883 (cert serveur) + ufw | 1 | cert + pinning firmware | ⚠ 4–8 h |
| mqtt-ingest | Python, 1 type | Python + routage par `model` + valid. schéma | 1 | schéma télémétrie | 4 h |
| Agrégation scalaire | `heatmap-agg` Python | continuous aggregates | 1 | hypertable | 6 h |
| alert-engine | Python, 30 s sur brut | Python, 5 min sur agrégats | 1 | agrégats | 2 h |
| Backups | aucun | tarball + DB chiffré age + git privé | 1 | — | 3 h |
| Bootstrap/restore | manuel | `bootstrap.sh` + `restore.sh` <20 min | 1 | secrets | 4 h |
| Claim/rattachement | `user_devices` sans flow | claim HMAC + pré-appairage + provisioning | 1 | FACTORY_SECRET | ⚠ 8 h |
| Firmware sécu | NVS clair (?) | flash encryption + NVS chiffré | 1 | modules de test dédiés | ⚠ 6 h |
| OTA | manuelle | Ed25519 + A/B + rollback + canaux | 2 | clé OTA, CI matrix | ⚠ 16 h+ |
| Secure Boot v2 | absent | activé | 2 | **OTA signée OK** | ⚠ 8 h |
| Agrégation heatmap | — | array agg fusion cellule | 2 | type `nl-map-in` | 8 h |
| Driver `nl-map-in` | — | HAL ToF matrice | 2 | core firmware | ⚠ |
| EMQX cluster | — | si scale | 3 | volume réel | — |

---

## 2. Détail des chantiers Bloc 1

Le Bloc 1 = ce qui rend `nl-presence` **vendable et déployable proprement**. Definition of Done par chantier.

### 2.1 Secrets générés
Script de bootstrap qui génère `JWT_SECRET`, `DB_PASSWORD`, mots de passe MQTT (32 B aléatoires), écrit `/etc/noisyless.env` (mode 600 root, `EnvironmentFile` systemd/compose). `FACTORY_SECRET` et clé privée OTA sauvegardés **hors VPS** (password manager) + backup chiffré. **DoD** : aucune valeur devinable, rien de commité, redéploiement reproductible.

### 2.2 Auth + ACL MQTT (Mosquitto)
`password_file` avec un compte par module (`username=DUID`, password = secret device hashé). `acl_file` régénéré par le backend à chaque claim/unclaim/changement d'abonnement → `noisyless/<duid>/#` pour le device, `read` pour le user propriétaire. Reload Mosquitto via sudoers dédié. **Piège R-20 EkoFlush** : `mosquitto_passwd -b` resserre les perms du fichier ; appliquer le workaround `chown noisyless:mosquitto + chmod 0640` (ou patcher la régénération). **DoD** : un module ne peut publier/lire que ses propres topics ; un abonnement annulé coupe la persistance après régénération.

### 2.3 TLS MQTT 8883
Cert serveur sur le domaine MQTT, listener 8883 TLS, `ufw` n'ouvre que 8883 en entrant pour ce flux. CA embarquée et **pinnée** côté firmware. mTLS (cert client par module) reporté — bon à avoir, pas bloquant. **DoD** : capture réseau = trafic chiffré, module rejette un cert non pinné.

### 2.4 Firmware : flash encryption + NVS chiffré
Flash encryption (release) + NVS chiffré pour creds WiFi et token device. **Garder des modules de dev séparés** (flash encryption release est irréversible — ne pas la subir sur le banc). Secure Boot **non** activé à ce stade (cf. décision §0). **DoD** : dump flash illisible, aucun secret en clair ni dans les logs.

### 2.5 Ingestion routée + agrégats scalaires
`mqtt-ingest` valide l'enveloppe + `metrics` contre le schéma `(model, schema)`, insère dans `measurements (time, duid, model, payload jsonb, health jsonb)`. Continuous aggregates pour les scalaires de `nl-presence`. **DoD** : un payload `nl-presence` invalide est rejeté proprement ; l'agrégat 5 min se rafraîchit seul.

### 2.6 Alert-engine
Worker qui poll/`LISTEN` les agrégats toutes les 5 min, compare aux seuils, notifie (Telegram + mail), anti-spam 1/fenêtre. **DoD** : pas de logique métier ni d'appel réseau dans la DB.

### 2.7 Claim / provisioning
HMAC `FACTORY_SECRET` (repris EkoFlush), pré-appairage au fulfillment + claim de secours, provisioning SoftAP captif (cf. doc dédié), mail de validation Brevo avant claim. **DoD** : module neuf branché → provisioning WiFi sans saisie de code → apparaît dans le bon compte.

### 2.8 Bootstrap + restore + backups
`bootstrap.sh` (secrets + init DB + schéma + hypertable), `restore.sh` (<20 min sur VPS vierge), `backup.sh` (DB `pg_dump` + configs → tarball chiffré age → rotation + push git privé). **DoD** : restauration testée sur une VM vierge.

---

## 3. Dépendances critiques (ne pas inverser)

1. **TLS MQTT avant d'exposer 8883** publiquement.
2. **OTA signée validée en réel avant Secure Boot v2** (sinon verrouillage parc).
3. **Modules de dev séparés avant flash encryption release** (irréversible).
4. **Schéma de télémétrie figé avant le routage d'ingestion** (sinon re-migration des données).

---

## 4. Hors périmètre Bloc 1 (ne pas anticiper)

EMQX, fleet rollout %, Grafana, multi-tenant billing, `nl-map-out` (IP67), 2FA admin, mTLS par module. Tout ça attend que `nl-presence` génère du CA. Drive the Tesla.

---

## 5. Scripts à créer (Bloc 1)

| Script | Rôle | Emplacement cible |
|--------|------|-------------------|
| `bootstrap.sh` | Génère secrets, init DB, schéma, hypertable | `/opt/noisyless/bootstrap.sh` |
| `restore.sh` | Restore DB + configs depuis backup | `/opt/noisyless/restore.sh` |
| `backup.sh` | Dump DB + configs → tarball chiffré → git | `/opt/noisyless/backup.sh` |
| `gen-secrets.sh` | Génère tous les secrets aléatoires | `/opt/noisyless/scripts/gen-secrets.sh` |
| `regen-mqtt-acl.sh` | Régénère `password_file` + `acl_file` | `/opt/noisyless/scripts/regen-mqtt-acl.sh` |
| `deploy-firmware.py` | Push OTA signée par model/canal | `/opt/noisyless/scripts/deploy-firmware.py` |

---

## 6. Risques identifiés

| Risque | Impact | Probabilité | Mitigation |
|--------|--------|-------------|------------|
| Flash encryption release sur module de dev | Perte du module | Moyenne | Modules dédiés, étiquetés "DEV" vs "PROD" |
| ACL MQTT mal régénérée | Modules coupés | Moyenne | Test staging + rollback ACL (git) |
| OTA signée buguée | Parc brické | Faible (si A/B) | Rollout beta 5% → 50% → 100% |
| `FACTORY_SECRET` perdu | Claim impossible | Faible | Backup hors-VPS + password manager |
| Tunnel Cloudflare down | Dashboard HS | Faible | SSL direct en fallback (garder cert) |

---

## 7. Critères de succès Bloc 1

- [ ] **Sécurité** : Audit `nmap` = seuls 8883 (MQTT) + tunnel (HTTP) ouverts
- [ ] **Auth** : Un module ne peut publier que sur `noisyless/<son_duid>/#`
- [ ] **Chiffrement** : Wireshark = MQTT illisible, NVS illisible
- [ ] **Déploiement** : `bootstrap.sh` + `restore.sh` testés sur VPS vierge (<20 min)
- [ ] **Claim** : Module neuf → provisioning SoftAP → dashboard en <5 min
- [ ] **OTA** : Push beta 5% → rollback auto si échec
- [ ] **Backups** : Restore testé avec données réelles

---

*Créé : 24 Mai 2026*  
*Statut : Prêt pour exécution Bloc 1*
