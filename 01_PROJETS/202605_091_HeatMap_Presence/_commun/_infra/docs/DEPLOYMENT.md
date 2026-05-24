# Déploiement NOISYLESS — Guide ultra-simple

**Niveau :** Débutant  
**Temps :** 30 minutes  
**Prérequis :** SSH + Docker

---

## Étape 1 : Se connecter au serveur

```bash
ssh -i ~/.ssh/id_ed25519 noisyless@91.99.26.43
```

**Si ça ne marche pas :**
```bash
# Vérifier que la clé SSH existe
ls -la ~/.ssh/id_ed25519

# Si elle n'existe pas, demander la clé au propriétaire
```

---

## Étape 2 : Vérifier Docker

```bash
docker --version
docker compose version
```

**Résultat attendu :**
```
Docker version 27.x.x
Docker Compose version v2.x.x
```

---

## Étape 3 : Cloner le projet

```bash
cd /opt
sudo git clone <URL_GIT> noisyless
sudo chown -R noisyless:noisyless noisyless
cd noisyless/_infra
```

**Si pas de Git :**
```bash
# Copier les fichiers en SCP depuis ta machine
scp -i ~/.ssh/id_ed25519 -r _infra/ noisyless@91.99.26.43:/opt/noisyless/
```

---

## Étape 4 : Générer les secrets

**⚠️ IMPORTANT :** Les secrets doivent être **aléatoires** et **sauvegardés hors du serveur**.

```bash
# Créer le fichier .env
cat > .env << 'EOF'
# Database
DB_PASSWORD=<GÉNÉRER_ALÉATOIRE_32_CARACTÈRES>

# JWT (authentification API)
JWT_SECRET=<GÉNÉRER_ALÉATOIRE_32_CARACTÈRES>
JWT_EXPIRY_MINUTES=30

# Telegram (alertes)
TELEGRAM_BOT_TOKEN=
TELEGRAM_CHAT_ID=

# Factory (claim devices)
FACTORY_SECRET=<GÉNÉRER_ALÉATOIRE_32_CARACTÈRES>
EOF

# Générer des secrets aléatoires
openssl rand -hex 16  # → donne 32 caractères aléatoires
```

**📝 Sauvegarder `.env` dans ton password manager (Bitwarden, 1Password, etc.)**

---

## Étape 5 : Démarrer les services

```bash
# Tout démarrer
docker compose up -d

# Vérifier que tout tourne
docker compose ps
```

**Résultat attendu :**
```
NAME                    STATUS
noisyless-mosquitto     Up
noisyless-timescaledb   Up (healthy)
noisyless-mqtt-ingest   Up
noisyless-heatmap-agg   Up
noisyless-alert-engine  Up
noisyless-api           Up
noisyless-nginx         Up
```

---

## Étape 6 : Vérifier que ça marche

### Test API
```bash
curl -k https://platform.noisyless.com/health
```

**Résultat attendu :**
```json
{"status": "ok", "service": "noisyless-api"}
```

### Test MQTT
```bash
# Se connecter au broker
docker exec -it noisyless-mosquitto mosquitto_sub -t '#' -v
```

**Résultat attendu :** (rien, c'est normal — attend des messages)

---

## Étape 7 : Configurer le DNS (Cloudflare)

1. Va sur https://dash.cloudflare.com
2. Sélectionne `noisyless.com`
3. DNS → Ajouter enregistrements :

| Type | Name | Content | Proxy |
|------|------|---------|-------|
| A | platform | 91.99.26.43 | DNS only (gris) |
| A | api.platform | 91.99.26.43 | DNS only (gris) |

4. Attendre 5-10 minutes

---

## Étape 8 : Générer le certificat SSL

```bash
cd /opt/noisyless

# Lancer certbot avec Cloudflare
docker run --rm \
  -v /opt/noisyless/nginx/ssl:/etc/letsencrypt \
  -v /opt/noisyless/cloudflare/cloudflare.ini:/cloudflare.ini:ro \
  certbot/dns-cloudflare certonly \
  -d platform.noisyless.com \
  -d api.platform.noisyless.com \
  --dns-cloudflare \
  --dns-cloudflare-credentials /cloudflare.ini \
  --email contact@noisyless.com \
  --agree-tos
```

**Si succès :**
```
Congratulations! Your certificate and chain have been saved.
```

### Copier les certificats au bon format
```bash
docker run --rm -v /opt/noisyless/nginx/ssl:/etc/letsencrypt alpine \
  cp /etc/letsencrypt/live/platform.noisyless.com/fullchain.pem \
     /etc/letsencrypt/platform.noisyless.com.crt

docker run --rm -v /opt/noisyless/nginx/ssl:/etc/letsencrypt alpine \
  cp /etc/letsencrypt/live/platform.noisyless.com/privkey.pem \
     /etc/letsencrypt/platform.noisyless.com.key
```

### Redémarrer Nginx
```bash
docker compose restart nginx
```

---

## Étape 9 : Tester HTTPS

```bash
curl -kI https://platform.noisyless.com
```

**Résultat attendu :**
```
HTTP/1.1 200 OK
Server: nginx/1.31.1
```

---

## Étape 10 : Configurer le renouvellement SSL

**Les certificats Let's Encrypt expirent après 90 jours.**  
Le renouvellement est **automatique** via un script cron.

```bash
# Vérifier que le cron est en place
ssh noisyless@91.99.26.43 "crontab -l"
```

**Résultat attendu :**
```
0 3 1 * * /opt/noisyless/renew-ssl.sh >> /var/log/ssl-renewal.log 2>&1
```

→ Renouvellement le 1er de chaque mois à 03:00

---

## 🆘 Problèmes courants

### "docker: command not found"
```bash
# Installer Docker
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker noisyless
```

### "Connection refused" sur le port 5432
```bash
# TimescaleDB n'est pas prêt
docker compose logs timescaledb

# Redémarrer
docker compose restart timescaledb
```

### Certificat SSL expiré
```bash
# Renouvellement manuel
/opt/noisyless/renew-ssl.sh
```

### MQTT ne reçoit rien
```bash
# Vérifier que le broker tourne
docker compose ps mosquitto

# Voir les logs
docker compose logs mosquitto
```

---

## 📞 Besoin d'aide ?

1. **Logs de tous les services :**
   ```bash
   docker compose logs -f
   ```

2. **Redémarrer un service :**
   ```bash
   docker compose restart <service>
   ```

3. **Voir l'espace disque :**
   ```bash
   df -h
   docker system df
   ```

4. **Nettoyer Docker :**
   ```bash
   docker system prune -a  # ⚠️ Supprime tout ce qui est arrêté
   ```

---

## ✅ Checklist de déploiement

- [ ] SSH vers serveur OK
- [ ] Docker installé
- [ ] Fichiers copiés sur `/opt/noisyless`
- [ ] `.env` généré avec secrets aléatoires
- [ ] Secrets sauvegardés hors serveur (password manager)
- [ ] `docker compose up -d` OK
- [ ] Tous services `Up`
- [ ] DNS Cloudflare configuré
- [ ] Certificat SSL généré
- [ ] HTTPS fonctionne (`curl -kI https://...`)
- [ ] Cron renouvellement en place

---

*Document créé : 24 Mai 2026*  
**Prochaine lecture :** `MAINTENANCE.md` (opérations courantes)
