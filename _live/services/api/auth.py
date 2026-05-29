"""
NOISYLESS — Authentication API
Register, Login, Email Verification, Password Reset
Uses OVH SMTP (contact@noisyless.com)
"""

import os
import asyncio
import asyncpg
import secrets
import hashlib
import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from datetime import datetime, timedelta
from typing import Optional
from fastapi import APIRouter, HTTPException, Depends, BackgroundTasks
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
from pydantic import BaseModel, EmailStr
from jose import jwt, JWTError
import bcrypt

# ── Config ──
DB_HOST = os.getenv("DB_HOST", "localhost")
DB_PORT = int(os.getenv("DB_PORT", "5432"))
DB_USER = os.getenv("DB_USER", "noisyless")
DB_PASSWORD = os.getenv("DB_PASSWORD", "noisyless_secret_password")
DB_NAME = os.getenv("DB_NAME", "noisyless")

JWT_SECRET = os.getenv("JWT_SECRET", secrets.token_urlsafe(32))
JWT_EXPIRY_MINUTES = int(os.getenv("JWT_EXPIRY_MINUTES", "30"))
JWT_ALGORITHM = "HS256"

# OVH SMTP (contact@noisyless.com)
SMTP_SERVER = os.getenv("SMTP_SERVER", "ssl0.ovh.net")
SMTP_PORT = int(os.getenv("SMTP_PORT", "465"))
SMTP_USER = os.getenv("SMTP_USER", "contact@noisyless.com")
SMTP_PASS = os.getenv("SMTP_PASS", "")

security = HTTPBearer()
def hash_password(pw): return bcrypt.hashpw(pw.encode(), bcrypt.gensalt()).decode()


router = APIRouter(prefix="", tags=["Authentication"])


# ── Models ──
class RegisterRequest(BaseModel):
    email: EmailStr
    password: str
    company: Optional[str] = None
    phone: Optional[str] = None


class LoginRequest(BaseModel):
    email: EmailStr
    password: str


class VerifyEmailRequest(BaseModel):
    email: EmailStr
    token: str


class PasswordResetRequest(BaseModel):
    email: EmailStr


class PasswordResetConfirmRequest(BaseModel):
    token: str
    new_password: str


class TokenResponse(BaseModel):
    access_token: str
    token_type: str = "bearer"
    expires_in: int


# ── Helpers ──
def create_access_token(data: dict, expires_delta: Optional[timedelta] = None) -> str:
    to_encode = data.copy()
    if expires_delta:
        expire = datetime.utcnow() + expires_delta
    else:
        expire = datetime.utcnow() + timedelta(minutes=JWT_EXPIRY_MINUTES)
    to_encode.update({"exp": expire})
    return jwt.encode(to_encode, JWT_SECRET, algorithm=JWT_ALGORITHM)


def verify_token(token: str) -> Optional[dict]:
    try:
        payload = jwt.decode(token, JWT_SECRET, algorithms=[JWT_ALGORITHM])
        return payload
    except JWTError:
        return None


def generate_verification_token() -> str:
    return secrets.token_urlsafe(32)


def send_email(to_email: str, subject: str, html_content: str):
    """Send email via OVH SMTP"""
    if not SMTP_PASS:
        print(f"[EMAIL] SMTP not configured. Would send to {to_email}")
        print(f"Subject: {subject}")
        return
    
    msg = MIMEMultipart('alternative')
    msg['Subject'] = subject
    msg['From'] = f"NOISYLESS <{SMTP_USER}>"
    msg['To'] = to_email
    
    # HTML part
    html = MIMEText(html_content, 'html')
    msg.attach(html)
    
    try:
        server = smtplib.SMTP_SSL(SMTP_SERVER, SMTP_PORT)
        server.login(SMTP_USER, SMTP_PASS)
        server.sendmail(SMTP_USER, to_email, msg.as_string())
        server.quit()
        print(f"[EMAIL] Sent to {to_email}: {subject}")
    except Exception as e:
        print(f"[EMAIL] Error: {e}")


async def send_verification_email(email: str, token: str):
    """Send email verification via OVH SMTP"""
    verification_url = f"https://platform.noisyless.com?email={email}&token={token}"
    
    html_content = f"""
    <html>
    <body style="font-family: Arial, sans-serif; line-height: 1.6; color: #333;">
        <div style="max-width: 600px; margin: 0 auto; padding: 20px;">
            <img src="https://noisyless.com/assets/logohorizmonochrome.png" alt="NOISYLESS" style="height: 40px; margin-bottom: 20px;">
            
            <h2 style="color: #4f6ef7;">Bienvenue sur NOISYLESS</h2>
            <p>Merci de vous être inscrit. Veuillez vérifier votre adresse email en cliquant sur le lien ci-dessous :</p>
            
            <p style="margin: 2rem 0;">
                <a href="{verification_url}" 
                   style="background: #4f6ef7; color: white; padding: 12px 24px; text-decoration: none; border-radius: 6px; display: inline-block;">
                    Vérifier mon email
                </a>
            </p>
            
            <p style="color: #666; font-size: 0.9rem;">
                Ou copiez-collez ce lien dans votre navigateur :<br>
                <a href="{verification_url}" style="color: #4f6ef7;">{verification_url}</a>
            </p>
            
            <hr style="border: none; border-top: 1px solid #eee; margin: 2rem 0;">
            
            <p style="color: #999; font-size: 0.85rem;">
                Si vous n'avez pas créé de compte, ignorez cet email.<br>
                © 2026 NOISYLESS — ELEC-CORE / MBHREP
            </p>
        </div>
    </body>
    </html>
    """
    
    send_email(email, "Vérifiez votre email - NOISYLESS", html_content)


async def send_reset_email(email: str, token: str):
    """Send password reset email via OVH SMTP"""
    reset_url = f"https://platform.noisyless.com/auth/reset?email={email}&token={token}"
    
    html_content = f"""
    <html>
    <body style="font-family: Arial, sans-serif; line-height: 1.6; color: #333;">
        <div style="max-width: 600px; margin: 0 auto; padding: 20px;">
            <img src="https://noisyless.com/assets/logohorizmonochrome.png" alt="NOISYLESS" style="height: 40px; margin-bottom: 20px;">
            
            <h2 style="color: #4f6ef7;">Réinitialisation du mot de passe</h2>
            <p>Vous avez demandé à réinitialiser votre mot de passe. Cliquez sur le lien ci-dessous :</p>
            
            <p style="margin: 2rem 0;">
                <a href="{reset_url}" 
                   style="background: #4f6ef7; color: white; padding: 12px 24px; text-decoration: none; border-radius: 6px; display: inline-block;">
                    Réinitialiser mon mot de passe
                </a>
            </p>
            
            <p style="color: #666; font-size: 0.9rem;">
                Ce lien expire dans 1 heure.<br>
                Si vous n'avez pas demandé cela, ignorez cet email.
            </p>
            
            <hr style="border: none; border-top: 1px solid #eee; margin: 2rem 0;">
            
            <p style="color: #999; font-size: 0.85rem;">
                © 2026 NOISYLESS — ELEC-CORE / MBHREP
            </p>
        </div>
    </body>
    </html>
    """
    
    send_email(email, "Réinitialisation du mot de passe - NOISYLESS", html_content)


async def send_welcome_email(email: str):
    """Send welcome email after verification"""
    html_content = f"""
    <html>
    <body style="font-family: Arial, sans-serif; line-height: 1.6; color: #333;">
        <div style="max-width: 600px; margin: 0 auto; padding: 20px;">
            <img src="https://noisyless.com/assets/logohorizmonochrome.png" alt="NOISYLESS" style="height: 40px; margin-bottom: 20px;">
            
            <h2 style="color: #22c55e;">Bienvenue ! 🎉</h2>
            <p>Votre email a été vérifié avec succès. Votre compte NOISYLESS est maintenant actif.</p>
            
            <h3 style="margin-top: 2rem; color: #4f6ef7;">Prochaines étapes :</h3>
            <ol style="line-height: 2;">
                <li>Connectez-vous au <a href="https://platform.noisyless.com" style="color: #4f6ef7;">Dashboard</a></li>
                <li>Enregistrez votre premier device (QR code ou code HMAC)</li>
                <li>Visualisez les données en temps réel</li>
            </ol>
            
            <p style="margin: 2rem 0;">
                <a href="https://platform.noisyless.com" 
                   style="background: #4f6ef7; color: white; padding: 12px 24px; text-decoration: none; border-radius: 6px; display: inline-block;">
                    Accéder au Dashboard
                </a>
            </p>
            
            <p style="color: #666; font-size: 0.9rem;">
                Besoin d'aide ? Répondez à cet email, nous sommes là pour vous accompagner.<br>
                L'équipe NOISYLESS
            </p>
            
            <hr style="border: none; border-top: 1px solid #eee; margin: 2rem 0;">
            
            <p style="color: #999; font-size: 0.85rem;">
                © 2026 NOISYLESS — ELEC-CORE / MBHREP
            </p>
        </div>
    </body>
    </html>
    """
    
    send_email(email, "Bienvenue sur NOISYLESS !", html_content)


# ── Endpoints ──
@router.post("/register", response_model=dict)
async def register(
    request: RegisterRequest,
    background_tasks: BackgroundTasks
):
    """Create a new user account"""
    import main
    async with main.db_pool.acquire() as conn:
        # Check if user exists
        existing = await conn.fetchrow(
            "SELECT id FROM noisyless.users WHERE email = $1",
            request.email
        )
        if existing:
            raise HTTPException(status_code=400, detail="Email already registered")
        
        # Hash password
        password_hash = hash_password(request.password)
        
        # Generate verification token
        verification_token = generate_verification_token()
        
        # Insert user
        user = await conn.fetchrow(
            """
            INSERT INTO noisyless.users 
            (email, password_hash, company, phone, verified, verification_token, created_at)
            VALUES ($1, $2, $3, $4, FALSE, $5, NOW())
            RETURNING id, email, created_at
            """,
            request.email,
            password_hash,
            request.company,
            request.phone,
            verification_token
        )
        
        # Send verification email
        background_tasks.add_task(send_verification_email, request.email, verification_token)
        
        return {
            "message": "Account created. Please check your email to verify.",
            "user_id": user["id"],
            "email": user["email"]
        }


@router.post("/login", response_model=TokenResponse)
async def login(request: LoginRequest):
    """Login and get JWT token"""
    import main
    async with main.db_pool.acquire() as conn:
        # Get user
        user = await conn.fetchrow(
            "SELECT id, email, password_hash, verified FROM noisyless.users WHERE email = $1",
            request.email
        )
        
        if not user:
            raise HTTPException(status_code=401, detail="Invalid credentials")
        
        # Verify password
        if not bcrypt.checkpw(request.password.encode(), user["password_hash"].encode()):
            raise HTTPException(status_code=401, detail="Invalid credentials")
        
        # Check if verified
        if not user["verified"]:
            raise HTTPException(
                status_code=403,
                detail="Email not verified. Please check your inbox."
            )
        
        # Create JWT token
        access_token = create_access_token(
            data={"sub": user["email"], "user_id": str(user["id"])},
            expires_delta=timedelta(minutes=JWT_EXPIRY_MINUTES)
        )
        
        return {
            "access_token": access_token,
            "token_type": "bearer",
            "expires_in": JWT_EXPIRY_MINUTES * 60
        }


@router.post("/verify")
async def verify_email(request: VerifyEmailRequest, background_tasks: BackgroundTasks):
    """Verify email address with token"""
    import main
    async with main.db_pool.acquire() as conn:
        # Check token
        user = await conn.fetchrow(
            """
            UPDATE noisyless.users 
            SET verified = TRUE, verification_token = NULL
            WHERE email = $1 AND verification_token = $2
            RETURNING id, email
            """,
            request.email,
            request.token
        )
        
        if not user:
            raise HTTPException(status_code=400, detail="Invalid verification token")
        
        # Send welcome email
        background_tasks.add_task(send_welcome_email, request.email)
        
        return {"message": "Email verified successfully"}


@router.post("/forgot-password")
async def forgot_password(
    request: PasswordResetRequest,
    background_tasks: BackgroundTasks
):
    """Request password reset email"""
    import main
    async with main.db_pool.acquire() as conn:
        user = await conn.fetchrow(
            "SELECT id, email FROM noisyless.users WHERE email = $1",
            request.email
        )
        
        if not user:
            # Don't reveal if email exists
            return {"message": "If the email exists, a reset link has been sent"}
        
        # Generate reset token
        reset_token = generate_verification_token()
        
        # Store token
        await conn.execute(
            "UPDATE noisyless.users SET verification_token = $1 WHERE email = $2",
            reset_token,
            request.email
        )
        
        # Send reset email
        background_tasks.add_task(send_reset_email, request.email, reset_token)
        
        return {"message": "If the email exists, a reset link has been sent"}


@router.post("/reset-password")
async def reset_password(request: PasswordResetConfirmRequest):
    """Reset password with token"""
    import main
    async with main.db_pool.acquire() as conn:
        # Hash new password
        password_hash = pwd_context.hash(request.new_password)
        
        # Update user
        updated = await conn.execute(
            """
            UPDATE noisyless.users 
            SET password_hash = $1, verification_token = NULL
            WHERE verification_token = $2
            """,
            password_hash,
            request.token
        )
        
        if updated == "UPDATE 0":
            raise HTTPException(status_code=400, detail="Invalid reset token")
        
        return {"message": "Password reset successfully"}


@router.get("/me")
async def get_current_user(
    credentials: HTTPAuthorizationCredentials = Depends(security)
):
    """Get current authenticated user"""
    payload = verify_token(credentials.credentials)
    if not payload:
        raise HTTPException(status_code=401, detail="Invalid token")
    
    import main
    async with main.db_pool.acquire() as conn:
        user = await conn.fetchrow(
            "SELECT id, email, company, phone, verified, created_at FROM noisyless.users WHERE id = $1",
            payload.get("user_id")
        )
        
        if not user:
            raise HTTPException(status_code=404, detail="User not found")
        
        return dict(user)
