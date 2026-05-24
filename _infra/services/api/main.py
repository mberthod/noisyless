"""
FastAPI Backend — NOISYLESS HeatMap #091
"""

import os
import asyncio
import asyncpg
from datetime import datetime, timedelta
from typing import Optional, List
from contextlib import asynccontextmanager

from fastapi import FastAPI, HTTPException, Depends
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
from pydantic import BaseModel
from jose import jwt
from passlib.context import CryptContext

# ── Config ──
DB_HOST = os.getenv("DB_HOST", "timescaledb")
DB_PORT = int(os.getenv("DB_PORT", "5432"))
DB_USER = os.getenv("DB_USER", "noisyless")
DB_PASSWORD = os.getenv("DB_PASSWORD", "noisyless_secret_password")
DB_NAME = os.getenv("DB_NAME", "noisyless")

JWT_SECRET = os.getenv("JWT_SECRET", "noisyless_jwt_super_secret_key_2026")
JWT_EXPIRY_MINUTES = int(os.getenv("JWT_EXPIRY_MINUTES", "30"))

security = HTTPBearer()
pwd_context = CryptContext(schemes=["bcrypt"], deprecated="auto")

db_pool = None


# ── Models ──
class Device(BaseModel):
    device_id: str
    device_type: str
    firmware_version: Optional[str] = None
    villa_id: Optional[str] = None


class HeatmapData(BaseModel):
    villa_id: str
    timestamp: datetime
    total_count: int
    max_count: int
    zones: dict


# ── Lifespan ──
@asynccontextmanager
async def lifespan(app: FastAPI):
    global db_pool
    db_pool = await asyncpg.create_pool(
        host=DB_HOST,
        port=DB_PORT,
        user=DB_USER,
        password=DB_PASSWORD,
        database=DB_NAME,
        min_size=2,
        max_size=10,
    )
    print("[API] DB pool created")
    yield
    await db_pool.close()


app = FastAPI(title="NOISYLESS HeatMap API", lifespan=lifespan)


# ── Auth Helpers ──
async def get_current_user(
    credentials: HTTPAuthorizationCredentials = Depends(security),
) -> str:
    """Validate JWT and return user email"""
    try:
        payload = jwt.decode(
            credentials.credentials,
            JWT_SECRET,
            algorithms=["HS256"],
        )
        return payload.get("sub")
    except Exception:
        raise HTTPException(status_code=401, detail="Invalid token")


# ── Endpoints ──
@app.get("/health")
async def health():
    """Health check"""
    return {"status": "ok", "service": "noisyless-api"}


@app.get("/api/devices")
async def list_devices(user: str = Depends(get_current_user)):
    """List user's devices"""
    async with db_pool.acquire() as conn:
        rows = await conn.fetch(
            """
            SELECT d.* FROM noisyless.devices d
            JOIN noisyless.user_devices ud ON d.id = ud.device_id
            JOIN noisyless.users u ON u.id = ud.user_id
            WHERE u.email = $1
            """,
            user,
        )
    return [dict(row) for row in rows]


@app.post("/api/devices")
async def create_device(
    device: Device,
    user: str = Depends(get_current_user),
):
    """Register a new device"""
    async with db_pool.acquire() as conn:
        # Get user ID
        user_row = await conn.fetchrow(
            "SELECT id FROM noisyless.users WHERE email = $1", user
        )
        if not user_row:
            raise HTTPException(status_code=404, detail="User not found")
        
        # Insert device
        device_row = await conn.fetchrow(
            """
            INSERT INTO noisyless.devices 
            (device_id, device_type, firmware_version, villa_id)
            VALUES ($1, $2, $3, $4)
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


@app.get("/api/devices/{device_id}/live")
async def get_device_live(
    device_id: str,
    user: str = Depends(get_current_user),
):
    """Get latest live data from device"""
    async with db_pool.acquire() as conn:
        row = await conn.fetchrow(
            """
            SELECT * FROM noisyless.measurements
            WHERE device_id = $1
            ORDER BY time DESC
            LIMIT 1
            """,
            device_id,
        )
    return dict(row) if row else None


@app.get("/api/devices/{device_id}/timeline")
async def get_device_timeline(
    device_id: str,
    hours: int = 24,
    user: str = Depends(get_current_user),
):
    """Get historical data for device"""
    async with db_pool.acquire() as conn:
        rows = await conn.fetch(
            """
            SELECT time, cluster_count, battery_mv, rssi_dbm
            FROM noisyless.measurements
            WHERE device_id = $1 AND time >= NOW() - INTERVAL '1 hour' * $2
            ORDER BY time DESC
            """,
            device_id,
            hours,
        )
    return [dict(row) for row in rows]


@app.get("/api/villas/{villa_id}/heatmap")
async def get_villa_heatmap(
    villa_id: str,
    user: str = Depends(get_current_user),
):
    """Get latest aggregated heatmap for villa"""
    async with db_pool.acquire() as conn:
        rows = await conn.fetch(
            """
            SELECT * FROM noisyless.heatmap_agg_5min
            WHERE villa_id = $1
            ORDER BY window_start DESC
            LIMIT 12
            """,
            villa_id,
        )
    return [dict(row) for row in rows]
