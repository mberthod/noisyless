"""NOISYLESS — Customer Check Endpoint
Check if a customer exists in Stripe and return their subscription info.
"""
import os
import stripe
from fastapi import APIRouter, HTTPException, Query

router = APIRouter()

STRIPE_SECRET_KEY = os.getenv("STRIPE_SECRET_KEY", "")
stripe.api_key = STRIPE_SECRET_KEY

@router.get("/api/customer/check")
async def check_customer(email: str = Query(...)):
    """
    Check if a customer exists in Stripe by email.
    Returns subscription info if found.
    """
    if not STRIPE_SECRET_KEY:
        raise HTTPException(status_code=500, detail="Stripe not configured")
    
    try:
        # Search for customer by email
        customers = stripe.Customer.list(email=email, limit=1)
        
        if not customers.data:
            return {"exists": False}
        
        customer = customers.data[0]
        
        # Get active subscriptions
        subscriptions = stripe.Subscription.list(customer=customer.id, status='active', limit=1)
        
        result = {
            "exists": True,
            "customer_id": customer.id,
            "email": customer.email,
            "name": customer.name,
            "subscription": None
        }
        
        if subscriptions.data:
            sub = subscriptions.data[0]
            # Extract plan from subscription items
            plan_name = "starter"  # default
            if sub.items.data:
                price = sub.items.data[0].price
                plan_name = price.metadata.get("plan", "starter")
            
            result["subscription"] = {
                "id": sub.id,
                "plan": plan_name,
                "quantity": sub.items.data[0].quantity if sub.items.data else 1,
                "status": sub.status
            }
        
        return result
    
    except stripe.error.StripeError as e:
        raise HTTPException(status_code=500, detail=f"Stripe error: {str(e)}")
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
