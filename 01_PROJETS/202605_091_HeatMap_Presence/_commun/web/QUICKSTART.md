# NOISYLESS Web Platform — Quickstart

**Niveau :** Débutant  
**Temps :** 1 heure  
**Objectif :** Avoir login/register + dashboard fonctionnel

---

## 📁 Structure créée

```
/home/mathieu/Kdrive/01_CLIENTS/NOISYLESS/
├── web/
│   ├── site/              # Site vitrine OVH
│   │   └── index.html     ✅ Existant (déjà en ligne)
│   │
│   ├── frontend/          # Dashboard client (Vue.js)
│   │   ├── src/
│   │   │   ├── views/
│   │   │   │   ├── Login.vue    # À créer
│   │   │   │   ├── Register.vue # À créer
│   │   │   │   └── Dashboard.vue# À créer
│   │   │   └── App.vue
│   │   └── public/
│   │
│   └── backend/           # API (FastAPI)
│       ├── api/
│       │   ├── main.py    ✅ Créé
│       │   ├── auth.py    ✅ Créé
│       │   └── devices.py ✅ Créé
│       └── requirements.txt
│
└── _infra/
    └── services/
        └── api/           # Backend existant (VPS)
```

---

## Étape 1 : Backend API (VPS)

### 1.1 Copier les fichiers sur le VPS

```bash
# Depuis ta machine locale
cd /home/mathieu/Kdrive/01_CLIENTS/NOISYLESS

# Copier backend vers VPS
scp -r backend/ noisyless@91.99.26.43:/opt/noisyless/

# Se connecter au VPS
ssh -i ~/.ssh/id_ed25519 noisyless@91.99.26.43
cd /opt/noisyless
```

### 1.2 Installer dépendances

```bash
# Dans le container API
docker exec -it noisyless-api pip install \
  fastapi uvicorn asyncpg \
  python-jose passlib \
  sib-api-v3-sdk

# Ou rebuild l'image
docker compose build api
docker compose up -d api
```

### 1.3 Configurer les variables d'environnement

```bash
# Éditer .env sur le VPS
cd /opt/noisyless
nano .env

# Ajouter :
BREVO_API_KEY=<ta_clé_brevo>
FACTORY_SECRET=<secret_aléatoire_32_caractères>
```

### 1.4 Redémarrer l'API

```bash
docker compose restart api
docker compose logs -f api
```

### 1.5 Tester les endpoints

```bash
# Health check
curl -k https://platform.noisyless.com/health

# Register (nouveau compte)
curl -X POST https://platform.noisyless.com/auth/register \
  -H "Content-Type: application/json" \
  -d '{"email": "test@example.com", "password": "MonMotDePasse123!"}'

# Login
curl -X POST https://platform.noisyless.com/auth/login \
  -H "Content-Type: application/json" \
  -d '{"email": "test@example.com", "password": "MonMotDePasse123!"}'
```

---

## Étape 2 : Frontend (Vue.js)

### 2.1 Créer le projet Vue

```bash
cd /home/mathieu/Kdrive/01_CLIENTS/NOISYLESS/web/frontend

# Initialiser Vue 3 + Vite
npm create vite@latest . -- --template vue

# Installer dépendances
npm install vue-router@4 pinia axios
```

### 2.2 Structure des composants

```
frontend/src/
├── views/
│   ├── Login.vue          # Formulaire login
│   ├── Register.vue       # Formulaire inscription
│   ├── VerifyEmail.vue    # Page vérification
│   └── Dashboard.vue      # Dashboard après login
├── components/
│   └── Navbar.vue
├── stores/
│   └── auth.js            # Pinia store (auth state)
├── router/
│   └── index.js           # Routes protégées
└── App.vue
```

### 2.3 Exemple : Login.vue

```vue
<template>
  <div class="min-h-screen flex items-center justify-center bg-gray-50">
    <div class="max-w-md w-full bg-white rounded-lg shadow-lg p-8">
      <h2 class="text-2xl font-bold text-center mb-6">Connexion</h2>
      
      <form @submit.prevent="handleLogin">
        <div class="mb-4">
          <label class="block text-gray-700 mb-2">Email</label>
          <input v-model="email" type="email" required
            class="w-full px-4 py-2 border rounded-lg focus:ring-2 focus:ring-emerald-500" />
        </div>
        
        <div class="mb-6">
          <label class="block text-gray-700 mb-2">Mot de passe</label>
          <input v-model="password" type="password" required
            class="w-full px-4 py-2 border rounded-lg focus:ring-2 focus:ring-emerald-500" />
        </div>
        
        <button type="submit"
          class="w-full py-3 bg-emerald-600 text-white rounded-lg hover:bg-emerald-700">
          Se connecter
        </button>
      </form>
      
      <p class="mt-4 text-center text-sm text-gray-600">
        Pas encore de compte ?
        <router-link to="/register" class="text-emerald-600 hover:underline">
          S'inscrire
        </router-link>
      </p>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

const email = ref('')
const password = ref('')
const router = useRouter()
const authStore = useAuthStore()

const handleLogin = async () => {
  try {
    await authStore.login(email.value, password.value)
    router.push('/dashboard')
  } catch (error) {
    alert('Erreur: ' + error.message)
  }
}
</script>
```

### 2.4 Auth Store (Pinia)

```js
// stores/auth.js
import { defineStore } from 'pinia'
import axios from 'axios'

const API_URL = 'https://platform.noisyless.com'

export const useAuthStore = defineStore('auth', {
  state: () => ({
    token: localStorage.getItem('token') || null,
    user: null
  }),
  
  getters: {
    isLoggedIn: (state) => !!state.token,
  },
  
  actions: {
    async login(email, password) {
      const response = await axios.post(`${API_URL}/auth/login`, {
        email, password
      })
      this.token = response.data.access_token
      localStorage.setItem('token', this.token)
      
      // Get user info
      const userResp = await axios.get(`${API_URL}/auth/me`, {
        headers: { Authorization: `Bearer ${this.token}` }
      })
      this.user = userResp.data
    },
    
    async register(email, password) {
      await axios.post(`${API_URL}/auth/register`, {
        email, password
      })
    },
    
    logout() {
      this.token = null
      this.user = null
      localStorage.removeItem('token')
    }
  }
})
```

---

## Étape 3 : Database Schema

### 3.1 Créer les tables

```bash
# Se connecter au VPS
ssh -i ~/.ssh/id_ed25519 noisyless@91.99.26.43

# Exécuter le SQL
docker exec -it noisyless-timescaledb psql -U noisyless -d noisyless << 'SQL'
-- Users table
CREATE TABLE IF NOT EXISTS noisyless.users (
    id SERIAL PRIMARY KEY,
    email TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    company TEXT,
    phone TEXT,
    verified BOOLEAN DEFAULT FALSE,
    verification_token TEXT,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Devices table
CREATE TABLE IF NOT EXISTS noisyless.devices (
    id SERIAL PRIMARY KEY,
    duid TEXT UNIQUE NOT NULL,
    device_type TEXT NOT NULL,
    firmware_version TEXT,
    villa_id TEXT,
    status TEXT DEFAULT 'online',
    owner_user_id INTEGER REFERENCES noisyless.users(id),
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- User-Devices link (many-to-many)
CREATE TABLE IF NOT EXISTS noisyless.user_devices (
    user_id INTEGER REFERENCES noisyless.users(id) ON DELETE CASCADE,
    device_id INTEGER REFERENCES noisyless.devices(id) ON DELETE CASCADE,
    PRIMARY KEY (user_id, device_id)
);

-- Index for faster lookups
CREATE INDEX IF NOT EXISTS idx_users_email ON noisyless.users(email);
CREATE INDEX IF NOT EXISTS idx_devices_duid ON noisyless.devices(duid);
SQL
```

---

## Étape 4 : Email (Brevo)

### 4.1 Créer un compte Brevo

1. Va sur https://www.brevo.com
2. Crée un compte gratuit (100 emails/jour)
3. Valide ton domaine `noisyless.com`
4. Crée une clé API (Settings → API Keys)

### 4.2 Configurer Brevo

```bash
# Sur le VPS
cd /opt/noisyless
nano .env

# Ajouter :
BREVO_API_KEY=xvz...ta_clé_brevo

# Redémarrer l'API
docker compose restart api
```

---

## ✅ Checklist finale

- [ ] Backend API copié sur VPS
- [ ] Dépendances installées (`pip install ...`)
- [ ] Variables d'environnement configurées (`.env`)
- [ ] Tables database créées (users, devices, user_devices)
- [ ] Endpoints testés (`/auth/register`, `/auth/login`)
- [ ] Frontend Vue.js créé
- [ ] Brevo configuré (emails de verification)
- [ ] HTTPS fonctionne sur `platform.noisyless.com`

---

## 🆘 Dépannage

### "ModuleNotFoundError: No module named 'sib_api_v3_sdk'"
```bash
docker exec noisyless-api pip install sib-api-v3-sdk
docker compose restart api
```

### "Connection refused" sur port 5432
```bash
docker compose logs timescaledb
docker compose restart timescaledb
```

### Email ne part pas
```bash
# Vérifier logs API
docker compose logs api | grep EMAIL

# Vérifier clé Brevo
docker exec noisyless-api env | grep BREVO
```

---

## 📞 Prochaines étapes

1. **Tester l'authentification complète** (register → email → login)
2. **Créer le dashboard** (voir ses devices + heatmap)
3. **Ajouter l'achat** (Stripe + webhook)
4. **Claim de device** (QR code + HMAC)

---

*Créé : 24 Mai 2026*
