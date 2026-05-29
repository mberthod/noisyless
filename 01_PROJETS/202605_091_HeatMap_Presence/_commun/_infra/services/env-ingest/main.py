"""
NOISYLESS Environmental Sensor Ingest — ESP32
Subscribe: esp32/noisyless/+/datas
Insert: measurements (TimescaleDB)
"""

import os
import sys
import json
import math
import asyncio
import asyncpg
import paho.mqtt.client as mqtt
from datetime import datetime, timezone
from typing import Optional

# ── Config ──────────────────────────────────────────────
MQTT_HOST = os.getenv("MQTT_HOST", "mosquitto")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_TOPIC = os.getenv("MQTT_TOPIC", "esp32/noisyless/+/datas")

DB_HOST = os.getenv("DB_HOST", "timescaledb")
DB_PORT = int(os.getenv("DB_PORT", "5432"))
DB_USER = os.getenv("DB_USER", "noisyless")
DB_PASSWORD = os.getenv("DB_PASSWORD", "noisyless_secret_password")
DB_NAME = os.getenv("DB_NAME", "noisyless")

# ADC → physical conversion
MIC_REF_PP = 1.0
MIC_DB_OFFSET = 30.0
MIC_DB_SCALE = 20.0
LUX_SCALE = 0.25

db_pool: Optional[asyncpg.Pool] = None
message_queue: asyncio.Queue = None
running = True


def mic_pp_to_db(pp: int) -> float:
    """Convert ADC peak-to-peak to approximate dB SPL."""
    if pp <= 0:
        return MIC_DB_OFFSET
    return MIC_DB_OFFSET + MIC_DB_SCALE * math.log10(max(pp, MIC_REF_PP))


def adc_to_lux(raw: int) -> float:
    """Convert raw ADC to approximate lux."""
    return round(raw * LUX_SCALE, 1)


async def init_db():
    global db_pool
    dsn = f"postgresql://{DB_USER}:{DB_PASSWORD}@{DB_HOST}:{DB_PORT}/{DB_NAME}"
    print(f"[DB] Connecting to {DB_HOST}:{DB_PORT}/{DB_NAME}...", flush=True)
    for attempt in range(5):
        try:
            db_pool = await asyncpg.create_pool(dsn, min_size=1, max_size=4)
            print(f"[DB] ✅ Pool created", flush=True)
            return
        except Exception as e:
            print(f"[DB] Attempt {attempt+1}/5 failed: {e}", flush=True)
            await asyncio.sleep(3)
    raise RuntimeError("Cannot connect to database after 5 attempts")


def on_connect(client, userdata, flags, rc, properties=None):
    print(f"[MQTT] on_connect rc={rc}", flush=True)
    if rc == 0:
        print(f"[MQTT] ✅ Connected to {MQTT_HOST}:{MQTT_PORT}", flush=True)
        client.subscribe(MQTT_TOPIC)
        print(f"[MQTT] ✅ Subscribed to {MQTT_TOPIC}", flush=True)
    else:
        print(f"[MQTT] ❌ Connection failed: rc={rc}", flush=True)


def on_message(client, userdata, msg):
    print(f"[MQTT] 📩 {msg.topic}", flush=True)
    if message_queue:
        message_queue.put_nowait((msg.topic, msg.payload))


def on_disconnect(client, userdata, rc, properties=None):
    print(f"[MQTT] Disconnected rc={rc}", flush=True)


async def process_message(topic: str, payload: bytes):
    try:
        data = json.loads(payload.decode("utf-8"))
        print(f"[MQTT] 📩 Decoded: {json.dumps(data, indent=2)[:200]}", flush=True)
        
        # Extract device info from topic: esp32/noisyless/<MAC>/datas
        parts = topic.split("/")
        mac = parts[2] if len(parts) > 2 else "unknown"
        device_id = data.get("device_id", f"NL-{mac.replace(':', '')[:6].upper()}")
        
        # Convert ADC → physical
        mic1_pp = data.get("sound_mic1_pp", 0)
        mic2_pp = data.get("sound_mic2_pp", 0)
        db_avg = mic_pp_to_db(mic1_pp)
        db_peak = mic_pp_to_db(max(mic1_pp, mic2_pp))
        lux1 = adc_to_lux(data.get("lux1_raw", 0))
        lux2 = adc_to_lux(data.get("lux2_raw", 0))
        
        if db_pool:
            async with db_pool.acquire() as conn:
                await conn.execute("""
                    INSERT INTO measurements (
                        time, device_id,
                        temperature_c, humidity_pct, gas_ohms, iaq,
                        presence, target_distance_cm,
                        sound_mic1_pp, sound_mic2_pp, lux1_raw, lux2_raw,
                        uptime_s, rssi_dbm
                    ) VALUES (NOW(), $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13)
                """,
                    device_id,
                    data.get("temperature_c"),
                    data.get("humidity_pct"),
                    data.get("gas_ohms"),
                    data.get("iaq"),
                    data.get("presence"),
                    data.get("target_distance_cm"),
                    mic1_pp,
                    mic2_pp,
                    data.get("lux1_raw", 0),
                    data.get("lux2_raw", 0),
                    data.get("uptime_s"),
                    data.get("rssi_dbm")
                )
                print(f"[DB] ✅ Inserted {device_id} | temp={data.get('temperature_c')}C iaq={data.get('iaq')} mic={mic1_pp}pp", flush=True)
                
                # Update device last_seen
                await conn.execute("""
                    INSERT INTO devices (device_id, device_type, firmware_version, status, last_seen, updated_at)
                    VALUES ($1, $2, $3, 'online', NOW(), NOW())
                    ON CONFLICT (device_id) DO UPDATE
                        SET status = 'online',
                            last_seen = NOW(),
                            firmware_version = EXCLUDED.firmware_version,
                            updated_at = NOW()
                """,
                    device_id,
                    'environmental',
                    data.get("fw_version")
                )
    except Exception as e:
        print(f"[ERROR] ❌ {e}", flush=True)
        import traceback
        traceback.print_exc()


async def message_processor():
    print("[PROCESSOR] Started", flush=True)
    while running:
        try:
            topic, payload = await asyncio.wait_for(message_queue.get(), timeout=1.0)
            await process_message(topic, payload)
        except asyncio.TimeoutError:
            continue
        except Exception as e:
            print(f"[PROCESSOR] Error: {e}", flush=True)


async def heartbeat_checker():
    """Mark devices offline if no data for 5 minutes"""
    OFFLINE_TIMEOUT = 300  # 5 minutes
    print("[HEARTBEAT] Started — timeout: 5 min", flush=True)
    while running:
        await asyncio.sleep(60)
        if db_pool:
            try:
                async with db_pool.acquire() as conn:
                    result = await conn.execute("""
                        UPDATE noisyless.devices
                        SET status = 'offline'
                        WHERE status = 'online'
                          AND last_seen < NOW() - INTERVAL '5 minutes'
                    """)
                    count = int(result.split()[-1]) if result else 0
                    if count > 0:
                        print(f"[HEARTBEAT] 🔴 {count} device(s) marked offline", flush=True)
            except Exception as e:
                print(f"[HEARTBEAT] Error: {e}", flush=True)


async def main():
    global running, message_queue

    message_queue = asyncio.Queue()

    await init_db()

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="noisyless-env-ingest")
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    client.reconnect_delay_set(min_delay=1, max_delay=30)

    print(f"[INIT] 🚀 Starting Environmental Ingest", flush=True)
    print(f"[INIT] MQTT: {MQTT_HOST}:{MQTT_PORT}", flush=True)
    print(f"[INIT] Topic: {MQTT_TOPIC}", flush=True)

    client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
    client.loop_start()

    processor_task = asyncio.create_task(message_processor())
    heartbeat_task = asyncio.create_task(heartbeat_checker())

    try:
        while running:
            await asyncio.sleep(1)
    except asyncio.CancelledError:
        pass
    finally:
        running = False
        client.loop_stop()
        client.disconnect()
        processor_task.cancel()
        heartbeat_task.cancel()
        if db_pool:
            await db_pool.close()
        print("[INIT] Clean shutdown.", flush=True)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[INIT] Interrupted by user.", flush=True)
