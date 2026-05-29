"""NOISYLESS — Admin API (firmware OTA, device management, users)"""
import os
import hashlib
import secrets
from pathlib import Path
from datetime import datetime, timezone
from fastapi import APIRouter, Depends, HTTPException, UploadFile, File, Form
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
from jose import jwt, JWTError
from typing import Optional, List
from pydantic import BaseModel

router = APIRouter(prefix="/admin", tags=["Admin"])
security = HTTPBearer()

FIRMWARE_DIR = Path("/opt/noisyless/firmware")
FIRMWARE_DIR.mkdir(parents=True, exist_ok=True)

JWT_SECRET = os.getenv("JWT_SECRET", "noisyless_jwt_super_secret_key_2026")

# ── Auth helpers ──

async def get_current_user(credentials: HTTPAuthorizationCredentials = Depends(security)):
    try:
        payload = jwt.decode(credentials.credentials, JWT_SECRET, algorithms=["HS256"])
        return payload
    except JWTError:
        raise HTTPException(status_code=401, detail="Invalid token")


async def get_db():
    import main
    return main.db_pool


async def require_admin(user: dict = Depends(get_current_user)):
    import main
    async with main.db_pool.acquire() as conn:
        row = await conn.fetchrow(
            "SELECT role FROM noisyless.users WHERE email = $1", user.get("sub")
        )
        if not row or row["role"] != "admin":
            raise HTTPException(status_code=403, detail="Admin access required")
    return user


# ── Models ──

class FirmwareUpload(BaseModel):
    version: str
    release_notes: Optional[str] = ""
    min_hardware_rev: Optional[str] = "1.0"

class OtaTrigger(BaseModel):
    firmware_version: str

class UserRoleUpdate(BaseModel):
    email: str
    role: str  # 'admin' or 'user'


# ── Devices (admin view — all devices) ──

@router.get("/devices")
async def list_all_devices(user: dict = Depends(require_admin)):
    db = await get_db()
    async with db.acquire() as conn:
        rows = await conn.fetch("""
            SELECT d.id, d.device_id, d.device_type, d.firmware_version,
                   d.firmware_sha256, d.target_firmware, d.villa_id,
                   d.status, d.last_seen, d.created_at, d.updated_at,
                   COUNT(ud.user_id) as user_count,
                   m.temperature_c as last_temperature,
                   m.humidity_pct as last_humidity,
                   m.pressure_pa as last_pressure
            FROM noisyless.devices d
            LEFT JOIN noisyless.user_devices ud ON d.id = ud.device_id
            LEFT JOIN LATERAL (
                SELECT temperature_c, humidity_pct, pressure_pa
                FROM noisyless.measurements
                WHERE device_id = d.device_id
                ORDER BY time DESC LIMIT 1
            ) m ON true
            GROUP BY d.id, d.device_id, m.temperature_c, m.humidity_pct, m.pressure_pa
            ORDER BY d.device_id ASC
        """)
    return [dict(r) for r in rows]


@router.get("/devices/{device_uuid}")
async def get_device_detail(device_uuid: str, user: dict = Depends(require_admin)):
    db = await get_db()
    async with db.acquire() as conn:
        device = await conn.fetchrow(
            "SELECT * FROM noisyless.devices WHERE id = $1", device_uuid
        )
        if not device:
            raise HTTPException(status_code=404, detail="Device not found")

        health = await conn.fetch(
            "SELECT * FROM noisyless.device_health WHERE device_id = $1 ORDER BY checked_at DESC LIMIT 20",
            device_uuid,
        )

        ota_history = await conn.fetch(
            "SELECT * FROM noisyless.ota_deployments WHERE device_id = $1 ORDER BY started_at DESC LIMIT 20",
            device_uuid,
        )

    return {
        "device": dict(device),
        "health": [dict(h) for h in health],
        "ota_deployments": [dict(o) for o in ota_history],
    }


# ── Users ──

@router.get("/users")
async def list_users(user: dict = Depends(require_admin)):
    db = await get_db()
    async with db.acquire() as conn:
        rows = await conn.fetch("""
            SELECT u.id, u.email, u.role, u.created_at,
                   COUNT(ud.device_id) as device_count
            FROM noisyless.users u
            LEFT JOIN noisyless.user_devices ud ON u.id = ud.user_id
            GROUP BY u.id
            ORDER BY u.created_at DESC
        """)
    return [dict(r) for r in rows]


@router.put("/users/role")
async def update_user_role(payload: UserRoleUpdate, user: dict = Depends(require_admin)):
    if payload.role not in ("admin", "user"):
        raise HTTPException(status_code=400, detail="Role must be 'admin' or 'user'")

    db = await get_db()
    async with db.acquire() as conn:
        result = await conn.execute(
            "UPDATE noisyless.users SET role = $1 WHERE email = $2",
            payload.role, payload.email,
        )
        if result == "UPDATE 0":
            raise HTTPException(status_code=404, detail="User not found")

    return {"email": payload.email, "role": payload.role, "updated": True}


# ── Firmware management ──

@router.post("/firmware/upload")
async def upload_firmware(
    version: str = Form(...),
    release_notes: str = Form(""),
    min_hardware_rev: str = Form("1.0"),
    file: UploadFile = File(...),
    user: dict = Depends(require_admin),
):
    if not file.filename or not file.filename.endswith(".bin"):
        raise HTTPException(status_code=400, detail="Only .bin firmware files accepted")

    content = await file.read()
    sha256 = hashlib.sha256(content).hexdigest()

    # Save to disk
    file_path = FIRMWARE_DIR / f"noisyless_{version}_{sha256[:8]}.bin"
    file_path.write_bytes(content)

    # Insert into DB
    db = await get_db()
    async with db.acquire() as conn:
        try:
            row = await conn.fetchrow(
                """INSERT INTO noisyless.firmware_versions
                   (version, file_path, sha256, release_notes, min_hardware_rev)
                   VALUES ($1, $2, $3, $4, $5) RETURNING *""",
                version, str(file_path), sha256, release_notes, min_hardware_rev,
            )
        except Exception as e:
            # Cleanup file on DB error
            file_path.unlink(missing_ok=True)
            if "unique" in str(e).lower():
                raise HTTPException(status_code=409, detail=f"Version {version} already exists")
            raise HTTPException(status_code=500, detail=str(e))

    return dict(row)


@router.get("/firmware/versions")
async def list_firmware_versions(user: dict = Depends(require_admin)):
    db = await get_db()
    async with db.acquire() as conn:
        rows = await conn.fetch(
            "SELECT * FROM noisyless.firmware_versions ORDER BY created_at DESC"
        )
    return [dict(r) for r in rows]


@router.get("/firmware/versions/{version}/download")
async def download_firmware(version: str):
    """Public endpoint — devices pull firmware via this URL"""
    db = await get_db()
    async with db.acquire() as conn:
        row = await conn.fetchrow(
            "SELECT file_path, sha256 FROM noisyless.firmware_versions WHERE version = $1",
            version,
        )
    if not row:
        raise HTTPException(status_code=404, detail="Version not found")

    from fastapi.responses import FileResponse
    return FileResponse(row["file_path"], media_type="application/octet-stream",
                        headers={"X-Firmware-SHA256": row["sha256"]})


# ── OTA deployments ──

@router.post("/devices/{device_uuid}/ota")
async def trigger_ota(
    device_uuid: str,
    payload: OtaTrigger,
    user: dict = Depends(require_admin),
):
    db = await get_db()
    async with db.acquire() as conn:
        # Check device exists
        device = await conn.fetchrow(
            "SELECT id, device_id FROM noisyless.devices WHERE id = $1", device_uuid
        )
        if not device:
            raise HTTPException(status_code=404, detail="Device not found")

        # Check firmware version exists
        fw = await conn.fetchrow(
            "SELECT version, sha256 FROM noisyless.firmware_versions WHERE version = $1",
            payload.firmware_version,
        )
        if not fw:
            raise HTTPException(status_code=404, detail="Firmware version not found")

        # Set target firmware on device
        await conn.execute(
            "UPDATE noisyless.devices SET target_firmware = $1 WHERE id = $2",
            payload.firmware_version, device_uuid,
        )

        # Create OTA deployment record
        deployment = await conn.fetchrow(
            """INSERT INTO noisyless.ota_deployments
               (firmware_version, device_id, status, started_at)
               VALUES ($1, $2, 'pending', NOW()) RETURNING *""",
            payload.firmware_version, device_uuid,
        )

    return {
        "deployment": dict(deployment),
        "message": f"OTA triggered: {device['device_id']} → {payload.firmware_version}"
    }


@router.get("/ota/deployments")
async def list_ota_deployments(user: dict = Depends(require_admin)):
    db = await get_db()
    async with db.acquire() as conn:
        rows = await conn.fetch("""
            SELECT od.*, d.device_id as device_name
            FROM noisyless.ota_deployments od
            JOIN noisyless.devices d ON od.device_id = d.id
            ORDER BY od.started_at DESC LIMIT 100
        """)
    return [dict(r) for r in rows]


# ── Dashboard stats ──

@router.get("/stats")
async def admin_stats(user: dict = Depends(require_admin)):
    db = await get_db()
    async with db.acquire() as conn:
        total_devices = await conn.fetchval("SELECT COUNT(*) FROM noisyless.devices")
        online_devices = await conn.fetchval(
            "SELECT COUNT(*) FROM noisyless.devices WHERE status = 'online'"
        )
        offline_devices = await conn.fetchval(
            "SELECT COUNT(*) FROM noisyless.devices WHERE status = 'offline'"
        )
        fw_count = await conn.fetchval("SELECT COUNT(*) FROM noisyless.firmware_versions")
        user_count = await conn.fetchval("SELECT COUNT(*) FROM noisyless.users")
        recent_deployments = await conn.fetch("""
            SELECT od.*, d.device_id as device_name
            FROM noisyless.ota_deployments od
            JOIN noisyless.devices d ON od.device_id = d.id
            ORDER BY od.started_at DESC LIMIT 10
        """)

    return {
        "total_devices": total_devices,
        "online_devices": online_devices,
        "offline_devices": offline_devices,
        "firmware_versions": fw_count,
        "user_count": user_count,
        "recent_deployments": [dict(r) for r in recent_deployments],
    }

# ── Measurements history ──

@router.get("/devices/{device_uuid}/measurements")
async def get_device_measurements(
    device_uuid: str,
    limit: int = 500,
    offset: int = 0,
    from_time: Optional[str] = None,
    to_time: Optional[str] = None,
    user: dict = Depends(require_admin),
):
    db = await get_db()
    async with db.acquire() as conn:
        # Resolve device_id from UUID
        device = await conn.fetchrow(
            "SELECT device_id FROM noisyless.devices WHERE id = $1", device_uuid
        )
        if not device:
            raise HTTPException(status_code=404, detail="Device not found")

        dev_id = device["device_id"]

        # Build query dynamically with optional time filters
        where_clauses = ["device_id = $1"]
        params = [dev_id]
        param_idx = 2

        if from_time:
            where_clauses.append(f"time >= ${param_idx}")
            try:
                params.append(datetime.fromisoformat(from_time))
            except ValueError:
                raise HTTPException(status_code=400, detail="Invalid from_time format, use ISO 8601")
            param_idx += 1
        if to_time:
            where_clauses.append(f"time <= ${param_idx}")
            try:
                params.append(datetime.fromisoformat(to_time))
            except ValueError:
                raise HTTPException(status_code=400, detail="Invalid to_time format, use ISO 8601")
            param_idx += 1

        where_sql = " AND ".join(where_clauses)

        # Total count
        count_query = f"SELECT COUNT(*) FROM noisyless.measurements WHERE {where_sql}"
        total = await conn.fetchval(count_query, *params)

        # Measurements with ORDER BY time DESC, LIMIT, OFFSET
        data_query = f"""
            SELECT time, device_id, villa_id, zone,
                   temperature_c, humidity_pct, pressure_pa, gas_ohms,
                   iaq, co2eq_ppm, bvoc_eq_ppm,
                   presence, people_count, target_distance_cm,
                   lux1_raw, lux2_raw,
                   sound_mic1_pp, sound_mic2_pp,
                   tof_grid, radar_targets, heatmap_cluster, cluster_count,
                   battery_mv, rssi_dbm, uptime_s
            FROM noisyless.measurements
            WHERE {where_sql}
            ORDER BY time DESC
            LIMIT ${param_idx} OFFSET ${param_idx + 1}
        """
        data_params = params + [limit, offset]
        rows = await conn.fetch(data_query, *data_params)

    return {
        "device_uuid": device_uuid,
        "device_id": dev_id,
        "total": total,
        "limit": limit,
        "offset": offset,
        "measurements": [dict(r) for r in rows],
    }
