"""NOISYLESS — Email module (SMTP OVH)"""
import os
import smtplib
import ssl
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
import logging

logger = logging.getLogger(__name__)

def send_order_confirmation(email: str, name: str, order_id: str, amount: float) -> bool:
    """Envoie un email de confirmation de commande via SMTP OVH."""
    smtp_host = os.getenv("SMTP_HOST", "ssl0.ovh.net")
    smtp_port = int(os.getenv("SMTP_PORT", "465"))
    smtp_user = os.getenv("SMTP_USER", "contact@noisyless.com")
    smtp_password = os.getenv("SMTP_PASSWORD", "")
    smtp_from = os.getenv("SMTP_FROM", "contact@noisyless.com")

    if not smtp_password:
        logger.error("SMTP_PASSWORD not set — cannot send confirmation email")
        return False

    order_short = order_id[:8] if len(order_id) >= 8 else order_id
    subject = f"Confirmation de commande NOISYLESS #{order_short}"

    msg = MIMEMultipart("alternative")
    msg["Subject"] = subject
    msg["From"] = f"NOISYLESS <{smtp_from}>"
    msg["To"] = email

    html = f"""\
<html>
  <body style="font-family:Arial,sans-serif;max-width:600px;margin:0 auto;padding:20px;">
    <h1 style="color:#1a1a2e;">Merci {name} !</h1>
    <p>Votre commande <strong>#{order_short}</strong> a bien été confirmée.</p>
    <p>Montant : <strong>{amount:.2f} €</strong></p>
    <p>Nous préparons votre NOISYLESS Simple. Vous recevrez un email dès l'expédition.</p>
    <hr>
    <p style="color:#666;font-size:12px;">NOISYLESS — Détection de bruit intelligente<br>contact@noisyless.com</p>
  </body>
</html>
"""

    msg.attach(MIMEText(html, "html"))

    try:
        context = ssl.create_default_context()
        with smtplib.SMTP_SSL(smtp_host, smtp_port, context=context) as server:
            server.login(smtp_user, smtp_password)
            server.sendmail(smtp_from, email, msg.as_string())
        logger.info(f"Confirmation email sent to {email} for order {order_short}")
        return True
    except Exception as e:
        logger.error(f"Failed to send confirmation email to {email}: {e}")
        return False
