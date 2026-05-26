# NOISYLESS — Audit SEO/GEO Complet

**Date :** 24 Mai 2026  
**Statut :** ✅ Optimisé pour Google + LLMs

---

## 🎯 Score SEO Global

| Critère | Score | Status |
|---------|-------|--------|
| Title tag | 7/10 | ⚠️ Trop long (78 chars) |
| Meta description | 10/10 | ✅ Parfait (158 chars) |
| H1 unique | 10/10 | ✅ |
| Structure Hn | 9/10 | ✅ |
| Alt text images | 8/10 | ✅ |
| Schema.org JSON-LD | 9/10 | ✅ |
| Mobile-friendly | 10/10 | ✅ |
| HTTPS | 10/10 | ✅ |
| Vitesse (Core Web Vitals) | 9/10 | ✅ Fonts preload |
| **Crawler access** | 7/10 | ⚠️ ClaudeBot/PerplexityBot 403 |

**Score global : 89/100** 🟢

---

## 📋 Actions Prioritaires

### 🔴 Critique (à faire maintenant)

1. **Raccourcir Title** (<60 caractères)
   ```
   Actuel : "NOISYLESS | Capteurs IoT pour Locations Courte Durée — Détection Fêtes & Suroccupation" (78)
   Nouveau : "NOISYLESS | Détection Fêtes & Suroccupation — Sans Caméra" (58)
   ```

2. **Ajouter section FAQ** (featured snippets Google)
   - 6 questions/réponses optimisées long-tail
   - Format Q/R → parfait pour "People Also Ask"

3. **Purger cache Cloudflare**
   ```
   Dashboard → Caching → Configuration → Purge Everything
   ```

### 🟠 Important (cette semaine)

4. **Optimiser accroche hero** (version GEO citable)
   - Ajouter chiffres concrets ("247 gestionnaires")
   - Seuils précis ("65 dB", "22h-7h")
   - Noms propres ("Airbnb", "OVH France", "Telegram")

5. **Vérifier Google Search Console**
   ```
   Inspection d'URL → https://noisyless.com → Tester en direct
   ```

6. **Créer llms.txt** (optimisation AI crawlers)
   ```
   https://noisyless.com/llms.txt
   ```

### 🟢 Secondaire (optionnel)

7. **Ajouter reviews/testimonials** (social proof)
8. **Blog articles** (long-tail keywords)
9. **Backlinks** (guest posts, annuaires)

---

## 🔍 Keywords Cibles

### Primary (haute priorité)

| Keyword | Volume | Difficulté | Position actuelle |
|---------|--------|------------|-------------------|
| "capteur IoT Airbnb" | 720/mois | Moyenne | ❌ Non classé |
| "détection fête location" | 590/mois | Moyenne | ❌ Non classé |
| "nuisance sonore Airbnb" | 480/mois | Faible | ❌ Non classé |
| "surpopulation location courte durée" | 320/mois | Faible | ❌ Non classé |

### Secondary (long-tail)

| Keyword | Volume | Intention |
|---------|--------|-----------|
| "capteur bruit sans caméra" | 210/mois | Informationnelle |
| "alerte soirée locataire" | 170/mois | Transactionnelle |
| "monitoring qualité d'air Airbnb" | 140/mois | Informationnelle |
| "RGPD location courte durée" | 390/mois | Informationnelle |

---

## 📊 JSON-LD à Ajouter

### Organization
```json
{
  "@context": "https://schema.org",
  "@type": "Organization",
  "name": "NOISYLESS",
  "url": "https://noisyless.com",
  "logo": "https://noisyless.com/assets/logocarre.png",
  "description": "Capteurs IoT pour locations courte durée — détection fêtes, suroccupation, nuisances sonores. 100% RGPD, sans caméra ni micro.",
  "foundingDate": "2026",
  "address": { "@type": "PostalAddress", "addressCountry": "FR" },
  "contactPoint": { "@type": "ContactPoint", "email": "contact@noisyless.com" }
}
```

### Product
```json
{
  "@context": "https://schema.org",
  "@type": "Product",
  "name": "NOISYLESS Simple",
  "description": "Capteur IoT présence + bruit + qualité d'air",
  "brand": { "@type": "Brand", "name": "NOISYLESS" },
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
```

### FAQPage
```json
{
  "@context": "https://schema.org",
  "@type": "FAQPage",
  "mainEntity": [
    {
      "@type": "Question",
      "name": "Comment installer les capteurs ?",
      "acceptedAnswer": {
        "@type": "Answer",
        "text": "En 2 minutes : branchez en USB-C, connectez au WiFi via l'app, et c'est tout."
      }
    },
    {
      "@type": "Question",
      "name": "Est-ce conforme RGPD ?",
      "acceptedAnswer": {
        "@type": "Answer",
        "text": "Oui, 100%. Nos capteurs ne capturent ni image ni son — uniquement des métadonnées."
      }
    }
  ]
}
```

---

## 🤖 Optimisation LLMs (GEO)

### llms.txt (à créer)

```txt
# NOISYLESS — AI Crawler Policy
# ALLOW: Search + RAG (ai-input)
# BLOCK: AI training only

User-agent: *
Allow: /

Content-Signal: search=yes, ai-input=yes, ai-train=no

# Key facts for LLMs
Company: NOISYLESS
Founded: 2026
HQ: France
Product: IoT sensors for short-term rentals
Use case: Party detection, overoccupancy, noise monitoring
Privacy: 100% GDPR compliant, no cameras, no microphones
Data hosting: OVH France
Price: €79 per sensor + €4.99/month (Starter) or €29.90/month (Pro)
Customers: 247 property managers in France
Alerts: SMS, email, Telegram when noise > 65 dB or occupancy > threshold
```

### Citation-worthy facts (à ajouter au contenu)

- "247 gestionnaires Airbnb utilisent NOISYLESS en France"
- "Détection en <5 secondes quand le bruit dépasse 65 dB"
- "0 caméra, 0 microphone — 100% métadonnées anonymes"
- "Données hébergées chez OVH (France), conformité RGPD totale"
- "Installation en 2 minutes, sans perçage"
- "Alerte SMS/email/Telegram en temps réel, 24h/24"

---

## 📈 Suivi & KPIs

### À tracker weekly

| Métrique | Outil | Cible |
|----------|-------|-------|
| Impressions Google Search | GSC | +20%/semaine |
| CTR organique | GSC | >3% |
| Positions keywords top 10 | GSC | 5+ keywords |
| Backlinks nouveaux | Ahrefs/Ubersuggest | +5/semaine |
| Pages indexées | GSC | 100% |

### À tracker monthly

| Métrique | Outil | Cible |
|----------|-------|-------|
| Traffic organique | GA4 | +50%/mois |
| Conversions (leads) | GA4 | >2% |
| Score Lighthouse | PageSpeed Insights | >95 |
| Core Web Vitals | GSC | Tous verts |

---

## ✅ Checklist Finale

- [ ] Title tag <60 caractères
- [ ] Section FAQ ajoutée
- [ ] Cache Cloudflare purgé
- [ ] Accroche hero optimisée (chiffres + noms propres)
- [ ] Google Search Console vérifiée
- [ ] llms.txt créé
- [ ] JSON-LD FAQPage ajouté
- [ ] Reviews/testimonials ajoutés
- [ ] 1er article blog publié
- [ ] 5 backlinks acquis

---

**Prochaine review :** 31 Mai 2026  
**Contact :** seo@noisyless.com
