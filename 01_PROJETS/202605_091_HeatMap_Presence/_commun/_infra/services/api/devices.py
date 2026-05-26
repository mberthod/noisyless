"""
NOISYLESS — Devices API
Device registration, claim, and management
"""

import os
import asyncpg
from fastapi import APIRouter, HTTPException, Depends
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
from pydantic import BaseModel
from typing import Optional, List

router = APIRouter(tags=["Devices"])


# ── Models ──
class DeviceCreate(BaseModel):
    device_id: str  # DUID (MAC address)
    device_type: str  # nl-presence, nl-map-in, nl-map-out
    firmware_version: Optional[str] = None
    villa_id: Optional[str] = None


class DeviceClaim(BaseModel):
    duid: str
    claim_code: str  # HMAC(F ACTORY_SECRET, DUID)


class DeviceResponse(BaseModel):
    id: int
    duid: str
    device_type: str
    firmware_version: Optional[str]
    villa_id: Optional[str]
    status: str
    owner_user_id: Optional[int]


# ── Helpers ──
async def get_current_user(
    credentials: HTTPAuthorizationCredentials = Depends(HTTPBearer())
) -> dict:
    """Validate JWT and return user info"""
    from jose import jwt, JWTError
    
    JWT_SECRET = os.getenv("JWT_SECRET", "noisyless_jwt_super_secret_key_2026")
    
    try:
        payload = jwt.decode(
            credentials.credentials,
            JWT_SECRET,
            algorithms=["HS256"],
        )
        return payload
    except JWTError:
        raise HTTPException(status_code=401, detail="Invalid token")


# ── Endpoints ──
@router.get("/devices", response_model=List[DeviceResponse])
async def list_devices(user: dict = Depends(get_current_user)):
    """List user's devices"""
    from .main import db_pool
    
    async with db_pool.acquire() as conn:
        rows = await conn.fetch(
            """
            SELECT d.* FROM noisyless.devices d
            JOIN noisyless.user_devices ud ON d.id = ud.device_id
            JOIN noisyless.users u ON u.id = ud.user_id
            WHERE u.email = $1
            """,
            user.get("sub")
        )
    return [dict(row) for row in rows]


@router.post("/devices", response_model=DeviceResponse)
async def create_device(
    device: DeviceCreate,
    user: dict = Depends(get_current_user)
):
    """Register a new device"""
    from .main import db_pool
    
    async with db_pool.acquire() as conn:
        # Get user ID
        user_row = await conn.fetchrow(
            "SELECT id FROM noisyless.users WHERE email = $1",
            user.get("sub")
        )
        if not user_row:
            raise HTTPException(status_code=404, detail="User not found")
        
        # Check if device already exists
        existing = await conn.fetchrow(
            "SELECT id FROM noisyless.devices WHERE duid = $1",
            device.device_id
        )
        if existing:
            raise HTTPException(status_code=400, detail="Device already registered")
        
        # Insert device
        device_row = await conn.fetchrow(
            """
            INSERT INTO noisyless.devices 
            (duid, device_type, firmware_version, villa_id, status, created_at)
            VALUES ($1, $2, $3, $4, 'online', NOW())
            RETURNING *
            """,
            device.device_id,
            device.device_type,
            device.firmware_version,
            device.villa_id,
        )
        
        # Link to user
        await conn.execute(
            "INSERT INTO noisyless.user_devices (user_id, device_id) VALUES ($1, $2)",
            user_row["id"],
            device_row["id"],
        )
        
        return dict(device_row)


@router.post("/devices/claim")
async def claim_device(
    claim: DeviceClaim,
    user: dict = Depends(get_current_user)
):
    """Claim a device with HMAC code"""
    from .main import db_pool
    import hmac
    import hashlib
    
    FACTORY_SECRET = os.getenv("FACTORY_SECRET", "change_me_in_production")
    
    # Verify HMAC
    expected = hmac.new(
        FACTORY_SECRET.encode(),
        claim.duid.encode(),
        hashlib.sha256
    ).hexdigest()
    
    if not hmac.compare_digest(claim.claim_code, expected):
        raise HTTPException(status_code=400, detail="Invalid claim code")
    
    async with db_pool.acquire() as conn:
        # Get user ID
        user_row = await conn.fetchrow(
            "SELECT id FROM noisyless.users WHERE email = $1",
            user.get("sub")
        )
        
        # Get or create device
        device = await conn.fetchrow(
            """
            INSERT INTO noisyless.devices (duid, status, created_at)
            VALUES ($1, 'online', NOW())
            ON CONFLICT (duid) DO UPDATE SET status = 'online'
            RETURNING *
            """,
            claim.duid
        )
        
        # Link to user (remove old owner if any)
        await conn.execute(
            "DELETE FROM noisyless.user_devices WHERE device_id = $1",
            device["id"]
        )
        
        await conn.execute(
            "INSERT INTO noisyless.user_devices (user_id, device_id) VALUES ($1, $2)",
            user_row["id"],
            device["id"],
        )
        
        return {"message": "Device claimed successfully", "device": dict(device)}
