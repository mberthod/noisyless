import os
import stripe
from fastapi import APIRouter, Request, HTTPException
import logging

# Setup basic logger if not present, crucial for the webhook's logging logic
logger = logging.getLogger(__name__)
if not logger.handlers:
    logger.setLevel(logging.INFO)
    # Check if the log directory exists/is needed, but for now, just define the logger setup minimally.

router = APIRouter() # Assuming the file uses an APIRouter instance

# --- Existing Shop/Product Routes (Placeholder/Assumption) ---
# Existing routes would go here, e.g.:
# @router.get("/products")
# async def list_products():
#     return {"products": []}

STRIPE_WEBHOOK_SECRET = os.getenv("STRIPE_WEBHOOK_SECRET", "")

@router.post("/webhook")
async def stripe_webhook(request: Request):
    """
    Reçoit les événements Stripe (webhook).
    Vérifie la signature, log les événements checkout.session.completed.
    """
    payload = await request.body()
    sig_header = request.headers.get("stripe-signature")

    if not sig_header:
        raise HTTPException(status_code=400, detail="Missing stripe-signature header")

    try:
        # Check if the secret is configured before attempting construction
        if not STRIPE_WEBHOOK_SECRET:
             logger.error("STRIPE_WEBHOOK_SECRET environment variable not set.")
             raise HTTPException(status_code=500, detail="Webhook secret not configured.")

        event = stripe.Webhook.construct_event(
            payload, sig_header, STRIPE_WEBHOOK_SECRET
        )
    except ValueError:
        logger.error("Invalid payload received.")
        raise HTTPException(status_code=400, detail="Invalid payload")
    except stripe.error.SignatureVerificationError:
        logger.error("Invalid Stripe signature.")
        raise HTTPException(status_code=400, detail="Invalid signature")

    # Gérer checkout.session.completed
    if event["type"] == "checkout.session.completed":
        session = event["data"]["object"]
        # Using logger.info as requested, assuming global setup handles it
        logger.info(f"💰 Paiement confirmé: session={session['id']}, email={session.get('customer_email', 'N/A')}, amount={session.get('amount_total', 0)}")
        # TODO: insérer dans la table orders une fois créée

    return {"status": "ok"}