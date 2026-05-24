"""
MQTT Ingest Service — NOISYLESS HeatMap #091
Subscribes to noisyless/+/heatmap and writes to TimescaleDB
"""

import os
import json
import asyncio
import asyncpg
import paho.mqtt.client as mqtt
from datetime import datetime
from typing import Optional

MQTT_HOST = os.getenv("MQTT_HOST", "mosquitto")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_TOPIC = "noisyless/+/heatmap"

DB_HOST = os.getenv("DB_HOST", "timescaledb")
DB_PORT = int(os.getenv("DB_PORT", "5432"))
DB_USER = os.getenv("DB_USER", "noisyless")
DB_PASSWORD = os.getenv("DB_PASSWORD", "noisyless_secret_password")
DB_NAME = os.getenv("DB_NAME", "noisyless")

db_pool: Optional[asyncpg.Pool] = None


def on_connect(client, userdata, flags, rc):
    print(f"[MQTT] on_connect called with rc={rc}")
    if rc == 0:
        print(f"[MQTT] ✅ Connected to {MQTT_HOST}:{MQTT_PORT}")
        client.subscribe(MQTT_TOPIC)
        print(f"[MQTT] ✅ Subscribed to {MQTT_TOPIC}")
    else:
        print(f"[MQTT] ❌ Connection failed: rc={rc}")


def on_message(client, userdata, msg):
    print(f"[MQTT] 📩 Message received: {msg.topic}")
    asyncio.create_task(process_message(msg.topic, msg.payload))


def on_disconnect(client, userdata, rc):
    print(f"[MQTT] Disconnected: rc={rc}")


async def process_message(topic: str, payload: bytes):
    try:
        data = json.loads(payload.decode("utf-8"))
        print(f"[MQTT] 📩 Decoded: {topic}")
        
        parts = topic.split("/")
        device_id = parts[1] if len(parts) > 1 else "unknown"
        villa_id = data.get("villa_id", "unassigned")
        
        if db_pool:
            async with db_pool.acquire() as conn:
                await conn.execute("""
                    INSERT INTO heatmap_data (time, device_id, villa_id, grid_json)
                    VALUES (NOW(), $1, $2, $3)
                """, device_id, villa_id, json.dumps(data))
                print(f"[DB] ✅ Inserted heatmap data for {device_id}")
    except Exception as e:
        print(f"[ERROR] ❌ Processing message: {e}")


async def init_db():
    global db_pool
    print(f"[DB] Connecting to {DB_HOST}:{DB_PORT}...")
    db_pool = await asyncpg.create_pool(
        host=DB_HOST, port=DB_PORT, user=DB_USER,
        password=DB_PASSWORD, database=DB_NAME,
        min_size=2, max_size=10
    )
    print("[DB] ✅ Pool created")


async def connect_mqtt_with_retry(max_retries=30, delay=2):
    """Connect to MQTT with retry logic (async version)"""
    for attempt in range(max_retries):
        try:
            print(f"[MQTT] 🔌 Connecting to {MQTT_HOST}:{MQTT_PORT} (attempt {attempt+1}/{max_retries})")
            
            client = mqtt.Client()
            client.on_connect = on_connect
            client.on_message = on_message
            client.on_disconnect = on_disconnect
            
            # Use asyncio-compatible connect
            await asyncio.get_event_loop().run_in_executor(
                None, lambda: client.connect(MQTT_HOST, MQTT_PORT, 60)
            )
            
            print("[MQTT] ✅ Connection successful")
            return client
        except Exception as e:
            print(f"[MQTT] ❌ Connection failed: {e}")
            if attempt < max_retries - 1:
                print(f"[MQTT] ⏳ Retrying in {delay}s...")
                await asyncio.sleep(delay)
            else:
                print("[MQTT] ❌ Max retries reached. Exiting.")
                raise
    
    return None


async def main():
    print("[INIT] 🚀 Starting MQTT Ingest Service")
    print(f"[INIT] MQTT: {MQTT_HOST}:{MQTT_PORT}")
    print(f"[INIT] DB: {DB_HOST}:{DB_PORT}/{DB_NAME}")
    
    # Wait for DB to be ready
    print("[INIT] Waiting for database...")
    await init_db()
    
    # Connect to MQTT with retry
    print("[INIT] Connecting to MQTT broker...")
    client = await connect_mqtt_with_retry()
    
    if not client:
        print("[INIT] ❌ Failed to connect to MQTT. Exiting.")
        return
    
    # Start MQTT loop (non-blocking)
    print("[INIT] Starting MQTT loop...")
    client.loop_start()
    
    # Keep running
    print("[INIT] ✅ Service started. Waiting for messages...")
    while True:
        await asyncio.sleep(60)


if __name__ == "__main__":
    asyncio.run(main())
