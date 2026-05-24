# PMS Integration Research — NOISYLESS HeatMap

> Objectif : corréler les données capteurs NOISYLESS (occupation, bruit, air) avec les calendriers de réservation pour valider/croiser les alertes.

---

## 1. Airbnb API

**Statut :** Écosystème fermé.

Airbnb n'expose pas d'API publique pour les property managers. Les options :

| Voie | Accès | Pertinence NOISYLESS |
|------|-------|---------------------|
| Airbnb API officielle | Partenariat certifié uniquement (OAuth 2.0) | ❌ Inaccessible hors channel manager agréé |
| Airbnb for Work | API entreprise, limitée aux déplacements pro | ❌ Hors scope |
| iCal export | Disponible pour tous les hôtes | ✅ Fallback disponibilité |

**Recommandation :** ne pas viser l'API Airbnb directe. Passer par un PMS partenaire (Guesty, Hostfully) ou utiliser iCal en fallback.

---

## 2. Guesty API

**Statut :** API mature, orientée développeurs.

Guesty est un PMS leader pour locations courte durée (50 000+ properties). Leur API REST permet :

- `GET /api/v1/reservations` — check-in/check-out, statut, nombre d'occupants déclaré
- `GET /api/v1/listings` — détails propriété, adresse
- Webhooks — événements en temps réel (réservation créée/modifiée/annulée)

| Métrique | Détail |
|----------|--------|
| **Auth** | API Key + Client Credentials flow |
| **Rate limits** | Variables selon abonnement Guesty |
| **Occupancy count** | Oui — champ `guests_count` dans la réservation |
| **Temps réel** | Oui — webhooks |
| **Pertinence** | **Très haute** |

**Recommandation :** Guesty est le partenaire PMS prioritaire. API la plus complète et la plus mature pour notre use case.

---

## 3. PMS Hubs / Agrégateurs

Ces plateformes connectent plusieurs PMS sources via une API unifiée :

| Hub | PMS supportés | API | Pertinence |
|-----|--------------|-----|------------|
| **Hostfully** | Airbnb, Vrbo, Booking.com, Guesty | API REST | Haute — agrégateur multi-PMS |
| **Rentals United** | 60+ PMS connectés | API REST | Haute — le plus large catalogue |
| **NextPax** | Focus Europe, multi-PMS | API REST | Moyenne |
| **BookingSync** | Locations saisonnières pro | API REST | Moyenne |
| **Wheelhouse** | Pricing + PMS data | API partielle | Faible (focus pricing) |

**Avantage hub :** une seule intégration NOISYLESS → n'importe quel PMS derrière. Le client garde son PMS existant.

**Recommandation :** prioriser Hostfully ou Rentals United comme couche d'abstraction PMS.

---

## 4. iCal — Fallback basique

**Disponible partout** (Airbnb, Booking.com, Vrbo, tous les PMS).

| Avantage | Limite |
|----------|--------|
| Gratuit, standard | Aucune meta-donnée structurée |
| Check-in/check-out | Pas de guest_count |
| URL publique | Pas de temps réel (pull 15-30 min) |
| Zéro auth | Pas de statut en cours de séjour |

**Usage NOISYLESS :** vérifier si le logement est censé être occupé aujourd'hui. Insuffisant pour croiser une alerte de suroccupation (besoin du guest_count). À utiliser uniquement en fallback si le PMS principal est down.

---

## 5. Architecture recommandée

```
NOISYLESS Dashboard
        │
        ▼
┌─────────────────┐
│  pms-bridge     │  ← Microservice NOISYLESS
│  (Python/FastAPI)│
└───────┬─────────┘
        │
   ┌────┴────┐
   ▼         ▼
┌──────┐  ┌──────────┐
│Guesty│  │Hostfully  │  ← PMS primaire (client existant)
│ API  │  │API (hub)  │
└──────┘  └──────────┘
   │         │
   │    (agrège Airbnb, Booking, etc.)
   │
   ▼
┌──────┐
│ iCal │  ← Fallback disponibilité uniquement
└──────┘
```

**Logique métier :**
1. Au check-in : NOISYLESS active le monitoring de la propriété
2. Pendant le séjour : corrélation guest_count déclaré vs occupancy mesurée → alerte si delta
3. Check-out : désactivation, rapport post-séjour

---

## 6. Données clés à extraire du PMS

| Champ | Usage NOISYLESS |
|-------|-----------------|
| `check_in` / `check_out` | Fenêtre de monitoring actif |
| `guest_count` | Seuil d'alerte suroccupation |
| `property_id` | Lien capteurs → logement |
| `status` (confirmed/cancelled/in_progress) | Activation/désactivation monitoring |

---

## 7. Actions

- [ ] Contacter Guesty pour accès API partenaire (developer.guesty.com)
- [ ] Évaluer Hostfully comme hub multi-PMS (hostfully.com/api)
- [ ] Prototype `pms-bridge` : client Guesty API → TimescaleDB
- [ ] iCal : parser basique de disponibilité (fallback, 1 journée de dev)
