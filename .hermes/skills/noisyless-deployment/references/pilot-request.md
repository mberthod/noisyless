# NOISYLESS — Pilot Request System

## Endpoint

**URL** : `POST https://platform.noisyless.com/api/pilot-request`

**Fichier** : `/opt/noisyless/services/api/pilot.py`

## Payload

```json
{
  "firstName": "Jean",
  "lastName": "Dupont",
  "email": "jean@conciergerie.com",
  "phone": "0612345678",
  "company": "Ma Conciergerie",
  "role": "conciergerie",
  "capteurs": "2-5",
  "biens": "15 locations Airbnb",
  "message": "Optionnel..."
}
```

**Champ `role`** — Valeurs valides :
- `proprietaire`
- `conciergerie`
- `gestionnaire`
- `syndic`
- `autre`

**Champ `capteurs`** — Valeurs valides :
- `1`
- `2-5`
- `6-10`
- `11-20`
- `20+`

## Réponses

**Succès (200)** :
```json
{"status": "ok", "message": "Demande envoyée"}
```

**Erreur (500)** :
```json
{"detail": "Erreur email: ..."}
```

## Emails envoyés

### 1. Email au client (confirmation)

**Destinataire** : `req.email`

**Sujet** : "Votre demande de pilote NOISYLESS"

**Contenu** :
- Confirmation de réception
- Récapitulatif de la demande (société, fonction, capteurs, biens)
- Délai de contact : 24-48h
- Liens vers FAQ et blog

### 2. Email à l'équipe (notification)

**Destinataire** : `TEAM_EMAIL` (défaut : `mathieu@mbhrep.com`)

**Sujet** : "📋 Nouveau pilote : {company} ({firstName} {lastName})"

**Contenu** :
- Tableau avec toutes les infos client
- Lien `mailto:` pré-rempli pour contacter le client
- Timestamp de réception

## Configuration SMTP

**Fichier** : `/opt/noisyless/.env`

```bash
# OVH SMTP (for transactional emails)
SMTP_SERVER=ssl0.ovh.net
SMTP_PORT=465
SMTP_USER=contact@noisyless.com
SMTP_PASSWORD=votre_mot_de_passe_ovh
SMTP_FROM="NOISYLESS <contact@noisyless.com>"

# Team notification
TEAM_EMAIL=mathieu@mbhrep.com
```

**Redémarrage requis** :
```bash
ssh noisyless@91.99.26.43
cd /opt/noisyless && docker compose restart api
```

## Test

```bash
curl -s -X POST https://platform.noisyless.com/api/pilot-request \
  -H "Content-Type: application/json" \
  -d '{
    "firstName":"Test",
    "lastName":"User",
    "email":"test@test.com",
    "phone":"0612345678",
    "company":"Test Corp",
    "role":"conciergerie",
    "capteurs":"1-5"
  }' --insecure
```

**Logs** :
```bash
ssh noisyless@91.99.26.43 "docker logs noisyless-api --tail 20" | grep -i "pilot\|email"
```

## Page formulaire

**URL** : `https://noisyless.com/pilote.html`

**Fichier** : `/opt/noisyless/static/pilote.html`

**Éléments du formulaire** :
- Prénom, Nom (requis)
- Email professionnel (requis)
- Téléphone (requis)
- Société (requis)
- Fonction (select, requis)
- Nombre de capteurs (select, requis)
- Nombre de biens (optionnel)
- Message (textarea, optionnel)

**Liens pointant vers `/pilote.html`** :
- Accueil : "Demander un pilote" (remplace `#contact`)
- FAQ : "Demander un pilote →" (remplace mailto:)

## Programme pilote — Ce qui est inclus

- 1 à 5 capteurs NOISYLESS Simple (selon parc)
- Accès dashboard complet pendant 30 jours
- Configuration et installation assistée
- Rapport d'analyse personnalisé en fin de pilote
- Offre de reprise : 100% du prix du pilote déduit d'un achat
