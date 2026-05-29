# _live/ — Snapshot production NOISYLESS v1

Ce dossier est un **instantané** (`snapshot`) de ce qui tourne réellement
sur le VPS Hetzner (91.99.26.43) à la date du 29 mai 2026.

## Structure

```
_live/
├── STATE.md               ← État des lieux complet (ce document)
├── docker-compose.yml     ← Compose utilisé en production
├── services/              ← Code source des micro-services
│   ├── api/               ← FastAPI (auth, devices, admin, shop...)
│   ├── env-ingest/        ← ESP32 capteurs environnement → DB
│   ├── mqtt-ingest/       ← HeatMap → DB
│   ├── heatmap-agg/       ← Agrégation 5 min
│   └── alert-engine/      ← Alertes seuils
├── nginx/
│   ├── nginx.conf         ← Config nginx (2 domaines, SSL, proxy)
│   └── nginx.conf.bak     ← Backup précédent
├── mosquitto/config/
│   └── mosquitto.conf     ← Config broker MQTT
└── static/                ← Pages web (html uniquement)
    ├── platform.html      ← Dashboard plateforme
    ├── admin.html         ← Panel admin
    ├── dashboard.html     ← Dashboard utilisateur
    ├── pilote-heatmap.html
    ├── precommande.html
    ├── conciergerie.html
    ├── produits/          ← heatmap-indoor, outdoor, property-pack
    ├── cas-usage/         ← conciergerie, hospitality, immobilier, villa
    ├── comparatif/        ← vs-camera, vs-detecteur, vs-minut
    ├── technologie/       ← intelligence-spatiale, privacy, reseau-maille
    └── firmware/          ← stable.json, latest.json
```

## Avertissement

Les fichiers dans `services/` et `static/` sont les sources déployées en
production. Les chemins d'infrastructure correspondants dans le repo
sont synchronisés avec `_infra/` et `_commun/web/site/`.

Pour toute modification : éditer dans `_infra/`, déployer sur VPS,
**puis** mettre à jour `_live/` en miroir.
