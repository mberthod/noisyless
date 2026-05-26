"""NOISYLESS — Shop webhook endpoint (Stripe events)"""
import os
import stripe
import json
import logging
from fastapi import APIRouter, Request, HTTPException
from emails import send_order_confirmation

logger = logging.getLogger(__name__)

router = APIRouter(prefix="/shop", tags=["Shop Webhook"])
stripe.api_key = os.getenv("STRIPE_SECRET_KEY", "")
STRIPE_WEBHOOK_SECRET = os.getenv("STRIPE_WEBHOOK_SECRET", "")

@router.post("/webhook")
async def stripe_webhook(request: Request):
    """Endpoint webhook Stripe — reçoit les événements post-paiement."""
    payload = await request.body()
    sig_header = request.headers.get("stripe-signature", "")

    try:
        if STRIPE_WEBHOOK_SECRET:
            event = stripe.Webhook.construct_event(payload, sig_header, STRIPE_WEBHOOK_SECRET)
        else:
            # Fallback pour test sans signature (NE PAS utiliser en prod)
            event = json.loads(payload)
    except (ValueError, stripe.error.SignatureVerificationError) as e:
        logger.error(f"Webhook signature verification failed: {e}")
        raise HTTPException(status_code=400, detail="Invalid signature")

    event_type = event.get("type", "")
    logger.info(f"Webhook received: {event_type}")

    if event_type == "checkout.session.completed":
        session = event["data"]["object"]
        email = session.get("customer_details", {}).get("email", "")
        name = session.get("customer_details", {}).get("name", session.get("metadata", {}).get("customer_name", "Client"))
        amount_total = session.get("amount_total", 0)
        session_id = session.get("id", "unknown")
        amount_eur = amount_total / 100.0

        if email:
            logger.info(f"Order completed: {session_id} — {email} — {amount_eur:.2f}€")
            success = send_order_confirmation(email, name, session_id, amount_eur)
            if not success:
                logger.warning(f"Confirmation email failed for {email} — order {session_id}")
        else:
            logger.warning(f"No email in session {session_id}, skipping confirmation")

        return {"status": "processed", "session_id": session_id}

    # Tous les autres événements sont ignorés
    return {"status": "ignored", "event": event_type}
