"""NOISYLESS — Pilot Request Endpoint
Handles pilot program requests and sends confirmation emails to both client and NOISYLESS team.
"""
import os
import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

router = APIRouter()

# OVH SMTP configuration
SMTP_SERVER = os.getenv("SMTP_SERVER", "ssl0.ovh.net")
SMTP_PORT = int(os.getenv("SMTP_PORT", "465"))
SMTP_USER = os.getenv("SMTP_USER", "contact@noisyless.com")
SMTP_PASS = os.getenv("SMTP_PASS", "")
SMTP_FROM = os.getenv("SMTP_FROM", "NOISYLESS <contact@noisyless.com>")

# Recipients
TEAM_EMAIL = os.getenv("TEAM_EMAIL", "mathieu@mbhrep.com")

class PilotRequest(BaseModel):
    firstName: str
    lastName: str
    email: str
    phone: str
    company: str
    role: str
    capteurs: str
    biens: str = ""
    message: str = ""

def send_email(to_email: str, subject: str, html_content: str, text_content: str = ""):
    """Send email via OVH SMTP"""
    if not SMTP_PASS:
        print(f"[EMAIL] SMTP not configured. Would send to {to_email}")
        return
    
    msg = MIMEMultipart("alternative")
    msg['Subject'] = subject
    msg['From'] = SMTP_FROM
    msg['To'] = to_email
    
    if text_content:
        msg.attach(MIMEText(text_content, 'plain', 'utf-8'))
    msg.attach(MIMEText(html_content, 'html', 'utf-8'))
    
    try:
        server = smtplib.SMTP_SSL(SMTP_SERVER, SMTP_PORT)
        server.login(SMTP_USER, SMTP_PASS)
        server.sendmail(SMTP_USER, to_email, msg.as_string())
        server.quit()
        print(f"[EMAIL] Sent to {to_email}: {subject}")
    except Exception as e:
        print(f"[EMAIL] Error: {e}")
        raise

@router.post("/api/pilot-request")
async def pilot_request(req: PilotRequest):
    """
    Handle pilot program request.
    Sends confirmation email to client AND notification email to NOISYLESS team.
    """
    
    # === EMAIL TO CLIENT (confirmation) ===
    client_subject = "Votre demande de pilote NOISYLESS"
    client_html = f"""
    <html>
    <body style="font-family: 'Space Mono', monospace; line-height: 1.6; color: #1A1A1A;">
        <p>Bonjour {req.firstName} {req.lastName},</p>
        
        <p>Nous avons bien reçu votre demande de pilote NOISYLESS.</p>
        
        <h3 style="font-family: 'Space Mono', monospace; margin-top: 24px;">Votre demande :</h3>
        <ul style="color: #5A5A5A;">
            <li><strong>Société :</strong> {req.company}</li>
            <li><strong>Fonction :</strong> {req.role}</li>
            <li><strong>Capteurs souhaités :</strong> {req.capteurs}</li>
            <li><strong>Biens équipés :</strong> {req.biens or 'Non spécifié'}</li>
        </ul>
        
        <p style="margin-top: 24px;">Notre équipe vous contactera sous <strong>24-48h</strong> pour :</p>
        <ul style="color: #5A5A5A;">
            <li>Valider votre éligibilité au programme pilote</li>
            <li>Configurer vos capteurs</li>
            <li>Planifier l'installation</li>
        </ul>
        
        <p style="margin-top: 24px;">En attendant, vous pouvez consulter notre <a href="https://noisyless.com/faq.html" style="color: #FF6B00;">FAQ</a> ou visiter notre <a href="https://noisyless.com/blog/" style="color: #FF6B00;">blog</a>.</p>
        
        <hr style="border: none; border-top: 1px solid #E0E0E0; margin: 32px 0;">
        
        <p style="color: #5A5A5A; font-size: 0.875rem;">
            Cordialement,<br>
            <strong>L'équipe NOISYLESS</strong><br>
            <a href="https://noisyless.com" style="color: #FF6B00;">noisyless.com</a>
        </p>
    </body>
    </html>
    """
    
    # === EMAIL TO TEAM (notification) ===
    team_subject = f"📋 Nouveau pilote : {req.company} ({req.firstName} {req.lastName})"
    team_html = f"""
    <html>
    <body style="font-family: 'Space Mono', monospace; line-height: 1.6; color: #1A1A1A;">
        <h2 style="font-family: 'Space Mono', monospace;">Nouvelle demande de pilote</h2>
        
        <table style="width: 100%; border-collapse: collapse; margin-top: 24px;">
            <tr>
                <td style="padding: 8px 0; border-bottom: 1px solid #E0E0E0;"><strong>Client :</strong></td>
                <td style="padding: 8px 0; border-bottom: 1px solid #E0E0E0;">{req.firstName} {req.lastName}</td>
            </tr>
            <tr>
                <td style="padding: 8px 0; border-bottom: 1px solid #E0E0E0;"><strong>Email :</strong></td>
                <td style="padding: 8px 0; border-bottom: 1px solid #E0E0E0;"><a href="mailto:{req.email}" style="color: #FF6B00;">{req.email}</a></td>
            </tr>
            <tr>
                <td style="padding: 8px 0; border-bottom: 1px solid #E0E0E0;"><strong>Téléphone :</strong></td>
                <td style="padding: 8px 0; border-bottom: 1px solid #E0E0E0;">{req.phone}</td>
            </tr>
            <tr>
                <td style="padding: 8px 0; border-bottom: 1px solid #E0E0E0;"><strong>Société :</strong></td>
                <td style="padding: 8px 0; border-bottom: 1px solid #E0E0E0;">{req.company}</td>
            </tr>
            <tr>
                <td style="padding: 8px 0; border-bottom: 1px solid #E0E0E0;"><strong>Fonction :</strong></td>
                <td style="padding: 8px 0; border-bottom: 1px solid #E0E0E0;">{req.role}</td>
            </tr>
            <tr>
                <td style="padding: 8px 0; border-bottom: 1px solid #E0E0E0;"><strong>Capteurs :</strong></td>
                <td style="padding: 8px 0; border-bottom: 1px solid #E0E0E0;">{req.capteurs}</td>
            </tr>
            <tr>
                <td style="padding: 8px 0; border-bottom: 1px solid #E0E0E0;"><strong>Biens :</strong></td>
                <td style="padding: 8px 0; border-bottom: 1px solid #E0E0E0;">{req.biens or 'Non spécifié'}</td>
            </tr>
        </table>
        
        {f'<h3 style="margin-top: 24px;">Message :</h3><p style="color: #5A5A5A; background: #F5F5F3; padding: 16px;">{req.message}</p>' if req.message else ''}
        
        <p style="margin-top: 32px;">
            <a href="mailto:{req.email}?subject=Votre pilote NOISYLESS" style="display: inline-block; padding: 12px 24px; background: #FF6B00; color: #FFFFFF; text-decoration: none; border-radius: 0;">Contacter le client →</a>
        </p>
        
        <hr style="border: none; border-top: 1px solid #E0E0E0; margin: 32px 0;">
        
        <p style="color: #5A5A5A; font-size: 0.75rem;">
            Reçu le {__import__('datetime').datetime.now().strftime('%d/%m/%Y à %H:%M')}
        </p>
    </body>
    </html>
    """
    
    try:
        # Send confirmation to client
        send_email(req.email, client_subject, client_html)
        
        # Send notification to team
        send_email(TEAM_EMAIL, team_subject, team_html)
        
        return {"status": "ok", "message": "Demande envoyée"}
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Erreur email: {str(e)}")
