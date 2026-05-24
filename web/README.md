# NOISYLESS — Web & Platform

**Structure :**
```
web/
├── site/              # Site vitrine OVH (noisyless.com)
│   ├── index.html     # Landing page (déjà en ligne)
│   └── assets/        # Images, CSS, JS
│
├── frontend/          # Dashboard client (platform.noisyless.com)
│   ├── src/
│   │   ├── components/
│   │   ├── views/
│   │   └── App.vue
│   └── public/
│
└── backend/           # API + Auth (VPS 91.99.26.43)
    ├── api/
    │   ├── main.py
    │   ├── auth.py    # Register, login, verify
    │   └── devices.py # CRUD devices
    └── services/
```

---

## 📋 État actuel

| Élément | URL | État |
|---------|-----|------|
| **Site vitrine** | `noisyless.com` | ✅ En ligne (OVH) |
| **Dashboard** | `platform.noisyless.com` | 🟡 Page statique (VPS) |
| **Auth API** | `api.platform.noisyless.com/auth/*` | ❌ À créer |

---

## 🚀 Architecture cible

```
┌─────────────────────────────────────────────────────┐
│  OVH Hosting (noisyless.com)                        │
│  — Site vitrine statique                            │
│  — Présentation produits                            │
│  — Bouton "Dashboard" → platform.noisyless.com      │
└─────────────────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────┐
│  VPS 91.99.26.43 (platform.noisyless.com)           │
│  ┌─────────────────────────────────────────────┐   │
│  │  Frontend (Vue.js + Tailwind)               │   │
│  │  — Login/Register                           │   │
│  │  — Dashboard client                         │   │
│  │  — Devices management                       │   │
│  └─────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────┐   │
│  │  Backend (FastAPI)                          │   │
│  │  — POST /auth/register                      │   │
│  │  — POST /auth/login                         │   │
│  │  — POST /auth/verify                        │   │
│  │  — GET  /api/devices                        │   │
│  │  — POST /api/devices                        │   │
│  └─────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────┐   │
│  │  Database (TimescaleDB)                     │   │
│  │  — users                                    │   │
│  │  — devices                                  │   │
│  │  — measurements                             │   │
│  └─────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

---

## 🔧 Prochaines étapes

### Semaine 1 — Authentification

1. **Backend API** (`backend/api/auth.py`)
   - [ ] `POST /auth/register` — Créer compte
   - [ ] `POST /auth/login` — Login + JWT
   - [ ] `POST /auth/verify` — Vérifier email
   - [ ] `POST /auth/forgot` — Reset password

2. **Frontend** (`frontend/src/views/Auth.vue`)
   - [ ] Page register
   - [ ] Page login
   - [ ] Page verify email
   - [ ] Dashboard après login

3. **Email (Brevo)**
   - [ ] Configurer API key Brevo
   - [ ] Template email verification
   - [ ] Template welcome

---

## 📊 Flux utilisateur

```
1. User arrive sur noisyless.com (OVH)
         │
         ▼
2. Clique "Dashboard" → platform.noisyless.com
         │
         ▼
3. Page login/register
         │
    ┌────┴────┐
    │         │
    ▼         ▼
4a. Login   4b. Register
    │         │
    ▼         ▼
5. JWT    5. Email verification
    │         │
    └────┬────┘
         ▼
6. Dashboard (voir ses devices + heatmap)
         │
         ▼
7. Acheter devices (Stripe)
         │
         ▼
8. Claim devices (QR code / HMAC)
```

---

## 🔗 Liens

- **Site OVH :** `ftp://noisyln@ftp.cluster121.hosting.ovh.net/www/`
- **VPS SSH :** `ssh -i ~/.ssh/id_ed25519 noisyless@91.99.26.43`
- **Infra docs :** `../_infra/docs/DEPLOYMENT.md`

---

*Créé : 24 Mai 2026*
