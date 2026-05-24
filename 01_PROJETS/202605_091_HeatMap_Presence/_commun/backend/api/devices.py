"""NOISYLESS — Devices API"""
import os
import asyncpg
from fastapi import APIRouter, HTTPException, Depends
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
from pydantic import BaseModel
from typing import Optional, List
from jose import jwt, JWTError

router = APIRouter(tags=["Devices"])
security = HTTPBearer()

async def get_current_user(credentials: HTTPAuthorizationCredentials = Depends(security)):
    JWT_SECRET = os.getenv("JWT_SECRET", "noisyless_jwt_super_secret_key_2026")
    try:
        payload = jwt.decode(credentials.credentials, JWT_SECRET, algorithms=["HS256"])
        return payload
    except JWTError:
        raise HTTPException(status_code=401, detail="Invalid token")

class DeviceCreate(BaseModel):
    device_id: str
    device_type: str
    firmware_version: Optional[str] = None
    villa_id: Optional[str] = None

class DeviceClaim(BaseModel):
    duid: str
    claim_code: str

@router.get("/devices")
async def list_devices(user: dict = Depends(get_current_user)):
    from .main import db_pool
    async with db_pool.acquire() as conn:
        rows = await conn.fetch(
            "SELECT d.* FROM noisyless.devices d JOIN noisyless.user_devices ud ON d.id = ud.device_id JOIN noisyless.users u ON u.id = ud.user_id WHERE u.email = $1",
            user.get("sub")
        )
    return [dict(row) for row in rows]

@router.post("/devices")
async def create_device(device: DeviceCreate, user: dict = Depends(get_current_user)):
    from .main import db_pool
    async with db_pool.acquire() as conn:
        user_row = await conn.fetchrow("SELECT id FROM noisyless.users WHERE email = $1", user.get("sub"))
        if not user_row:
            raise HTTPException(status_code=404, detail="User not found")
        device_row = await conn.fetchrow(
            "INSERT INTO noisyless.devices (duid, device_type, firmware_version, villa_id, status, created_at) VALUES ($1, $2, $3, $4, 'online', NOW()) RETURNING *",
            device.device_id, device.device_type, device.firmware_version, device.villa_id
        )
        await conn.execute("INSERT INTO noisyless.user_devices (user_id, device_id) VALUES ($1, $2)", user_row["id"], device_row["id"])
    return dict(device_row)

@router.post("/devices/claim")
async def claim_device(claim: DeviceClaim, user: dict = Depends(get_current_user)):
    from .main import db_pool
    import hmac, hashlib
    FACTORY_SECRET = os.getenv("FACTORY_SECRET", "change_me")
    expected = hmac.new(FACTORY_SECRET.encode(), claim.duid.encode(), hashlib.sha256).hexdigest()
    if not hmac.compare_digest(claim.claim_code, expected):
        raise HTTPException(status_code=400, detail="Invalid claim code")
    async with db_pool.acquire() as conn:
        user_row = await conn.fetchrow("SELECT id FROM noisyless.users WHERE email = $1", user.get("sub"))
        device = await conn.fetchrow("INSERT INTO noisyless.devices (duid, status, created_at) VALUES ($1, 'online', NOW()) ON CONFLICT (duid) DO UPDATE SET status = 'online' RETURNING *", claim.duid)
        await conn.execute("DELETE FROM noisyless.user_devices WHERE device_id = $1", device["id"])
        await conn.execute("INSERT INTO noisyless.user_devices (user_id, device_id) VALUES ($1, $2)", user_row["id"], device["id"])
    return {"message": "Device claimed", "device": dict(device)}
