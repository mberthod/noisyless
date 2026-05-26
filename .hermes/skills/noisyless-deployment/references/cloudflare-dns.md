# Cloudflare DNS & Cache — NOISYLESS

## Configuration DNS

| Type | Name | Content | Proxy | TTL |
|------|------|---------|-------|-----|
| A | noisyless.com | 91.99.26.43 | DNS only (gris) | Auto |
| A | platform | 91.99.26.43 | DNS only (gris) | Auto |
| A | www | 91.99.26.43 | Proxied (orange) | Auto |
| AAAA | noisyless.com | 2001:41d0:301::21 | Proxied | Auto |

**Important** :
- `noisyless.com` (site vitrine) → **DNS only** pour éviter le cache edge
- `platform.noisyless.com` (app) → peut être Proxied
- `www` → Proxied avec redirect vers naked domain

## Vérification DNS

```bash
# Vérifier la résolution
dig noisyless.com +short
# Doit retourner : 91.99.26.43

# Vérifier si Cloudflare proxy est actif
curl -s https://noisyless.com/cdn-cgi/trace | grep -E "^(ip|colo)"
# Si colo=XXX → Cloudflare proxy actif
# Si pas de réponse → DNS only
```

## Problème : Contenu ne se met pas à jour

**Symptôme** :
```bash
curl https://noisyless.com/ | grep pk_
# Retourne pk_test_YOUR_STRIPE_PUBLIC_KEY (ancienne version)
```

**Mais le serveur a la bonne version** :
```bash
ssh noisyless@91.99.26.43 "grep pk_ /opt/noisyless/static/index.html"
# Retourne pk_live_... (bonne version)
```

**Solutions** :

### 1. Désactiver le proxy Cloudflare (recommandé)

Dashboard Cloudflare → DNS → `A noisyless.com` :
- Cliquer sur le nuage **orange** → passer en **gris** (DNS only)
- Attendre 5-15 min propagation
- Tester :
  ```bash
  curl https://noisyless.com/ | grep pk_live
  ```

### 2. Purger le cache Cloudflare

Dashboard Cloudflare → Caching → Configuration :
- **Purge Everything**
- Attendre 30 secondes

### 3. Forcer bypass cache (test)

```bash
curl -s https://noisyless.com/ \
  -H "Cache-Control: no-cache" \
  -H "Pragma: no-cache" \
  | grep pk_
```

### 4. Test direct IP (bypass DNS)

```bash
curl -s --resolve noisyless.com:443:91.99.26.43 https://noisyless.com/ --insecure | grep pk_
```

## Propagation DNS

**Délais** :
- DNS only : 5-15 min (souvent immédiat)
- Avec proxy Cloudflare : jusqu'à 48h (rarement plus de 4h)

**Forcer la propagation** :
1. Dashboard Cloudflare → DNS
2. Modifier l'enregistrement (ajouter un espace dans la note)
3. Sauvegarder
4. Ça force une refresh immédiate

## Revenir en mode Proxied

Après confirmation que le site fonctionne :

1. Dashboard Cloudflare → DNS
2. `A noisyless.com` → Cliquer nuage **gris** → passer **orange**
3. Purger cache
4. Tester

**Avantages Proxied** :
- Protection DDoS
- CDN edge
- SSL automatique

**Inconvénients** :
- Cache edge peut retarder les déploiements
- Nécessite purge manuelle après chaque deploy
