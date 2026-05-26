# Stripe Checkout Flow — NOISYLESS

## Endpoints

### POST /shop/checkout

**Request** :
```json
{
  "items": [
    {"product_id": "nl-simple", "quantity": 1}
  ],
  "email": "customer@example.com",
  "name": "Customer Name",
  "company": "Optional Company",
  "success_url": "https://noisyless.com/merci.html",
  "cancel_url": "https://noisyless.com/annule.html"
}
```

**Response** :
```json
{
  "session_id": "cs_live_abc123...",
  "url": "https://checkout.stripe.com/c/pay/cs_live_..."
}
```

**Backend** : `/opt/noisyless/services/api/shop.py`

### Frontend Integration

**Clé publique LIVE** :
```javascript
const stripe = Stripe('pk_live_51TTSCbEvgqhDyN6IaNiWvkC6rlxg2JkKZb6iwXmXKzLTdQsAVFVFBTKJYOCgEBMa2NYN4unCQTUA2fzoy8zTZcyF00mbGKDUEb');
```

**Code checkout** :
```javascript
async function startCheckout(e) {
  e.preventDefault();
  const email = document.getElementById('shopEmail').value;
  const name = document.getElementById('shopName').value;

  const response = await fetch('https://platform.noisyless.com/shop/checkout', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      items: [{ product_id: 'nl-simple', quantity: 1 }],
      email: email,
      name: name,
      success_url: window.location.origin + '/merci',
      cancel_url: window.location.origin + '/annule'
    })
  });

  if (response.ok) {
    const session = await response.json();
    if (session.url) {
      window.location.href = session.url;  // Redirection directe
    } else if (session.session_id) {
      const stripe = Stripe('pk_live_...');
      await stripe.redirectToCheckout({ sessionId: session.session_id });
    }
  }
}
```

## Produits configurés

**Fichier** : `/opt/noisyless/services/api/shop.py`

```python
PRODUCTS = {
    "nl-simple": {"id": "nl-simple", "name": "NOISYLESS Simple", "price": 7900, "currency": "eur", "available": True},
    "nl-map-in": {"id": "nl-map-in", "name": "HeatMap Intérieur", "price": 12900, "available": False},
    "nl-map-out": {"id": "nl-map-out", "name": "HeatMap Extérieur", "price": 19900, "available": False},
    "nl-leak": {"id": "nl-leak", "name": "Détecteur Fuite + Vanne", "price": 14900, "available": False}
}
```

## Test

```bash
curl -s -X POST https://noisyless.com/shop/checkout \
  -H "Content-Type: application/json" \
  -d '{"items":[{"product_id":"nl-simple","quantity":1}],"email":"test@test.com","name":"Test"}' \
  --insecure | jq '.url'
```

**Résultat attendu** : URL Stripe Checkout valide commençant par `https://checkout.stripe.com/c/pay/cs_live_...`

## Pages de confirmation

| Page | URL | Fichier |
|------|-----|---------|
| Succès | `/merci.html` | `/opt/noisyless/static/merci.html` |
| Annulé | `/annule.html` | `/opt/noisyless/static/annule.html` |

**Note** : Les deux pages ont `<meta name="robots" content="noindex">` pour éviter l'indexation.
