# .hermes/ — Kit de démarrage agent Hermes pour NOISYLESS

## Pour un agent sur une nouvelle machine

```bash
# 1. Cloner le repo
git clone git@github.com:mberthod/noisyless.git
cd noisyless

# 2. Installer les skills Hermes
cp -r .hermes/skills/* ~/.hermes-mbhrep/skills/

# 3. Installer la knowledge base
cp -r .hermes/knowledge/* ~/.hermes-mbhrep/knowledge/

# 4. Installer le backlog
cp -r .hermes/backlog/* ~/.hermes-mbhrep/backlog/

# 5. Lire le SOUL.md
cat .hermes/SOUL.md
```

## Secrets (NE PAS COMMITER)

Le fichier `.env` de production est sur le serveur : `noisyless@91.99.26.43:/opt/noisyless/.env`

Structure (`.env.example` dans `01_PROJETS/202605_091_HeatMap_Presence/_commun/_infra/`) :
```
DB_PASSWORD=...
STRIPE_SECRET_KEY=sk_live_...
STRIPE_PUBLISHABLE_KEY=pk_live_...
SMTP_PASSWORD=...
FACTORY_SECRET=...
JWT_SECRET=...
```

## Structure

```
.hermes/
├── SOUL.md                    ← Instructions système pour l'agent
├── skills/
│   ├── noisyless-heatmap/     ← Règles firmware/hardware #091
│   └── noisyless-deployment/  ← Procédures déploiement
├── backlog/
│   ├── todo.json              ← 28 tâches avec statuts
│   └── logs/                  ← Historique d'exécution (23 logs)
├── knowledge/
│   └── elec-core/
│       ├── clients/           ← Fiche client NOISYLESS
│       └── projects/          ← Fiches projets #045, #089, #091
└── README.md                  ← Ce fichier
```

## Déploiement rapide

```bash
# Déployer le site vitrine
rsync -avz 01_PROJETS/202605_091_HeatMap_Presence/_commun/web/site/ noisyless@91.99.26.43:/opt/noisyless/static/

# Rebuild API après modif
ssh noisyless@91.99.26.43 "cd /opt/noisyless && docker compose build api && docker compose up -d api"

# Purger cache Cloudflare (manuel via dashboard)
# https://dash.cloudflare.com → noisyless.com → Caching → Purge Everything
```
