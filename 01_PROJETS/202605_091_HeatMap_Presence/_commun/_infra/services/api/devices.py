"""
NOISYLESS — Devices API
Device registration, claim, and management
"""

import os, hmac, hashlib
import asyncpg
from fastapi import APIRouter, HTTPException, Depends
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
from pydantic import BaseModel
from typing import Optional, List

router = APIRouter(tags=["Devices"])


# ── Models ──
class DeviceCreate(BaseModel):
    device_id: str
    device_type: str
    firmware_version: Optional[str] = None
    villa_id: Optional[str] = None


class DeviceClaim(BaseModel):
    duid: str
    claim_code: str


# ── Helpers ──
async def get_current_user(
    credentials: HTTPAuthorizationCredentials = Depends(HTTPBearer())
) -> dict:
    from jose import jwt, JWTError
    JWT_SECRET = os.getenv("JWT_SECRET", "noisyless_jwt_super_secret_key_2026")
    try:
        payload = jwt.decode(credentials.credentials, JWT_SECRET, algorithms=["HS256"])
        return payload
    except JWTError:
        raise HTTPException(status_code=401, detail="Invalid token")


# ── Endpoints ──
@router.get("/devices")
async def list_devices(user: dict = Depends(get_current_user)):
    """List user's devices"""
    import main
    async with main.db_pool.acquire() as conn:
        rows = await conn.fetch(
            """
            SELECT d.* FROM noisyless.devices d
            JOIN noisyless.user_devices ud ON d.id = ud.device_id
            JOIN noisyless.users u ON u.id = ud.user_id
            WHERE u.email = $1
            """,
            user.get("sub")
        )
        result = []
        for row in rows:
            item = {}
            for k, v in dict(row).items():
                if hasattr(v, 'isoformat'):
                    item[k] = v.isoformat()
                elif isinstance(v, bytes):
                    item[k] = v.decode('utf-8', errors='replace')
                else:
                    item[k] = str(v) if v is not None else None
            result.append(item)
        return result


@router.post("/devices")
async def create_device(
    device: DeviceCreate,
    user: dict = Depends(get_current_user)
):
    """Register a new device"""
    import main
    async with main.db_pool.acquire() as conn:
        user_row = await conn.fetchrow(
            "SELECT id FROM noisyless.users WHERE email = $1",
            user.get("sub")
        )
        if not user_row:
            raise HTTPException(status_code=404, detail="User not found")

        existing = await conn.fetchrow(
            "SELECT id FROM noisyless.devices WHERE device_id = $1",
            device.device_id
        )
        if existing:
            raise HTTPException(status_code=400, detail="Device already registered")

        device_row = await conn.fetchrow(
            """
            INSERT INTO noisyless.devices
            (device_id, device_type, firmware_version, villa_id, created_at)
            VALUES ($1, $2, $3, $4, NOW())
            RETURNING *
            """,
            device.device_id, device.device_type,
            device.firmware_version, device.villa_id
        )

        await conn.execute(
            "INSERT INTO noisyless.user_devices (user_id, device_id) VALUES ($1, $2)",
            user_row["id"], device_row["id"]
        )

        item = {}
        for k, v in dict(device_row).items():
            if hasattr(v, 'isoformat'): item[k] = v.isoformat()
            elif isinstance(v, bytes): item[k] = v.decode('utf-8', errors='replace')
            else: item[k] = str(v) if v is not None else None
        return item


@router.post("/devices/claim")
async def claim_device(
    claim: DeviceClaim,
    user: dict = Depends(get_current_user)
):
    """Claim a device with HMAC code"""
    import main
    FACTORY_SECRET = os.getenv("FACTORY_SECRET", "change_me_in_production")
    expected = hmac.new(FACTORY_SECRET.encode(), claim.duid.encode(), hashlib.sha256).hexdigest()

    if not hmac.compare_digest(claim.claim_code, expected):
        raise HTTPException(status_code=400, detail="Invalid claim code")

    async with main.db_pool.acquire() as conn:
        user_row = await conn.fetchrow(
            "SELECT id FROM noisyless.users WHERE email = $1",
            user.get("sub")
        )
        device = await conn.fetchrow(
            """
            INSERT INTO noisyless.devices (device_id, created_at)
            VALUES ($1, NOW())
            ON CONFLICT (device_id) DO UPDATE SET updated_at = NOW()
            RETURNING *
            """,
            claim.duid
        )
        await conn.execute(
            "DELETE FROM noisyless.user_devices WHERE device_id = $1",
            device["id"]
        )
        await conn.execute(
            "INSERT INTO noisyless.user_devices (user_id, device_id) VALUES ($1, $2)",
            user_row["id"], device["id"]
        )
        return {"message": "Device claimed successfully", "device": dict(device)}


# ── Measurement endpoints ──
@router.get("/devices/{device_id}/latest")
async def get_latest_measurement(device_id: str, user: dict = Depends(get_current_user)):
    """Get latest measurement for a device"""
    import main
    async with main.db_pool.acquire() as conn:
        row = await conn.fetchrow(
            """
            SELECT * FROM measurements
            WHERE device_id = $1
            ORDER BY time DESC
            LIMIT 1
            """,
            device_id
        )
        if not row:
            raise HTTPException(status_code=404, detail="No measurements found")
        item = {}
        for k, v in dict(row).items():
            if hasattr(v, 'isoformat'): item[k] = v.isoformat()
            elif isinstance(v, bytes): item[k] = v.decode('utf-8', errors='replace')
            else: item[k] = v
        return item


@router.put("/devices/{device_id}/settings")
async def update_device_settings(
    device_id: str,
    payload: dict,
    user: dict = Depends(get_current_user)
):
    import main
    allowed = {"device_name", "installation_address", "alert_threshold_db",
               "alert_threshold_co2", "alert_threshold_temp_min",
               "alert_threshold_temp_max"}
    updates = {k: v for k, v in payload.items() if k in allowed}
    if not updates:
        raise HTTPException(status_code=400, detail="No valid fields")
    set_clause = ", ".join(f"{k} = ${i+2}" for i, k in enumerate(updates.keys()))
    values = list(updates.values())
    async with main.db_pool.acquire() as conn:
        row = await conn.fetchrow(
            f"UPDATE noisyless.devices SET {set_clause}, updated_at = NOW() WHERE device_id = $1 RETURNING *",
            device_id, *values
        )
        if not row:
            raise HTTPException(status_code=404, detail="Device not found")
        item = {}
        for k, v in dict(row).items():
            if hasattr(v, "isoformat"):
                item[k] = v.isoformat()
            elif isinstance(v, bytes):
                item[k] = v.decode("utf-8", errors="replace")
            else:
                item[k] = str(v) if v is not None else None
        return item


@router.post("/devices/{device_id}/detach")
async def detach_device(
    device_id: str,
    user: dict = Depends(get_current_user)
):
    import main
    async with main.db_pool.acquire() as conn:
        device = await conn.fetchrow(
            "SELECT id FROM noisyless.devices WHERE device_id = $1", device_id
        )
        if not device:
            raise HTTPException(status_code=404, detail="Device not found")
        await conn.execute(
            "DELETE FROM noisyless.user_devices "
            "WHERE device_id = $1 AND user_id = (SELECT id FROM noisyless.users WHERE email = $2)",
            device["id"], user.get("sub")
        )
    return {"status": "detached", "device_id": device_id}
