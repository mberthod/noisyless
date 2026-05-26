"""NOISYLESS — Shop API with Stripe"""
import os
import stripe
from fastapi import APIRouter, HTTPException
from pydantic import BaseModel
from typing import Optional, List, Literal

stripe.api_key = os.getenv("STRIPE_SECRET_KEY", "")

router = APIRouter(prefix="/shop", tags=["Shop"])

PRODUCTS = {
    "nl-simple": {"id": "nl-simple", "name": "NOISYLESS Simple", "description": "Présence + bruit + qualité d'air", "price": 7900, "currency": "eur", "available": True, "type": "hardware"},
    "nl-map-in": {"id": "nl-map-in", "name": "HeatMap Intérieur", "description": "Cartographie ToF 16×16", "price": 12900, "currency": "eur", "available": False, "type": "preorder"},
    "nl-map-out": {"id": "nl-map-out", "name": "HeatMap Extérieur", "description": "Radar mmWave 60 GHz", "price": 19900, "currency": "eur", "available": False, "type": "preorder"},
    "nl-leak": {"id": "nl-leak", "name": "Détecteur Fuite + Vanne", "description": "Détection eau + coupure auto", "price": 14900, "currency": "eur", "available": False, "type": "preorder"}
}

SUBSCRIPTIONS = {
    "starter": {"id": "starter", "name": "Starter", "description": "Pour 1 à 5 logements", "price": 499, "currency": "eur"},
    "pro": {"id": "pro", "name": "Pro", "description": "Gestionnaires de parc", "price": 2990, "currency": "eur"}
}

class CartItem(BaseModel):
    product_id: str
    quantity: int = 1
    subscription_plan: Optional[Literal["starter", "pro"]] = None

class CheckoutRequest(BaseModel):
    items: List[CartItem]
    email: str
    name: str
    company: Optional[str] = None
    success_url: str = "https://noisyless.com/shop/success"
    cancel_url: str = "https://noisyless.com/shop/cancel"

@router.get("/products")
async def list_products():
    return {"products": list(PRODUCTS.values()), "subscriptions": list(SUBSCRIPTIONS.values())}

@router.post("/checkout")
async def create_checkout(request: CheckoutRequest):
    if not stripe.api_key:
        raise HTTPException(status_code=500, detail="Stripe not configured")
    line_items = []
    for item in request.items:
        if item.product_id not in PRODUCTS:
            raise HTTPException(status_code=400, detail="Product not found")
        product = PRODUCTS[item.product_id]
        line_items.append({
            "price_data": {
                "currency": product["currency"],
                "product_data": {"name": product["name"], "description": product["description"], "metadata": {"type": product["type"], "product_id": product["id"]}},
                "unit_amount": product["price"],
            },
            "quantity": item.quantity,
        })
    if request.items[0].subscription_plan:
        plan = SUBSCRIPTIONS.get(request.items[0].subscription_plan)
        if plan:
            line_items.append({
                "price_data": {
                    "currency": plan["currency"],
                    "product_data": {"name": f"Abonnement {plan['name']}", "description": plan["description"]},
                    "recurring": {"interval": "month"},
                    "unit_amount": plan["price"],
                },
                "quantity": 1,
            })
    session = stripe.checkout.Session.create(
        payment_method_types=["card"],
        line_items=line_items,
        mode="payment" if not request.items[0].subscription_plan else "subscription",
        customer_email=request.email,
        success_url=request.success_url + "?session_id={CHECKOUT_SESSION_ID}",
        cancel_url=request.cancel_url,
        metadata={"customer_name": request.name, "customer_company": request.company or ""},
    )
    return {"session_id": session.id, "url": session.url}

@router.get("/checkout/status/{session_id}")
async def checkout_status(session_id: str):
    try:
        session = stripe.checkout.Session.retrieve(session_id)
        return {"status": session.payment_status, "customer_email": session.customer_email, "amount_total": session.amount_total, "metadata": session.metadata}
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))
