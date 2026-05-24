"""
MQTT Ingest Service — NOISYLESS HeatMap #091
Subscribes to noisyless/+/heatmap and writes to TimescaleDB
"""

import os
import sys
import json
import asyncio
import asyncpg
import paho.mqtt.client as mqtt
from datetime import datetime, timezone
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
message_queue: asyncio.Queue = None
running = True


def on_connect(client, userdata, flags, rc):
    print(f"[MQTT] on_connect called with rc={rc}", flush=True)
    if rc == 0:
        print(f"[MQTT] ✅ Connected to {MQTT_HOST}:{MQTT_PORT}", flush=True)
        client.subscribe(MQTT_TOPIC)
        print(f"[MQTT] ✅ Subscribed to {MQTT_TOPIC}", flush=True)
    else:
        print(f"[MQTT] ❌ Connection failed: rc={rc}", flush=True)


def on_message(client, userdata, msg):
    print(f"[MQTT] 📩 Message received: {msg.topic}", flush=True)
    print(f"[MQTT] 📩 Payload: {msg.payload}", flush=True)
    if message_queue:
        message_queue.put_nowait((msg.topic, msg.payload))
        print(f"[MQTT] ✅ Message queued", flush=True)


def on_disconnect(client, userdata, rc):
    print(f"[MQTT] Disconnected: rc={rc}", flush=True)


async def process_message(topic: str, payload: bytes):
    try:
        data = json.loads(payload.decode("utf-8"))
        print(f"[MQTT] 📩 Decoded: {data}", flush=True)
        
        parts = topic.split("/")
        device_id = parts[1] if len(parts) > 1 else "unknown"
        villa_id = data.get("villa_id", "unassigned")
        zone = data.get("zone", "default")
        total_count = data.get("total_count", 0)
        max_count = data.get("max_count", 0)
        grid_data = data.get("grid_json", data.get("heatmap", {}))
        
        if db_pool:
            async with db_pool.acquire() as conn:
                await conn.execute("""
                    INSERT INTO heatmap_agg_5min (window_start, villa_id, zone, total_count, max_count, grid_data)
                    VALUES (NOW(), $1, $2, $3, $4, $5)
                """, villa_id, zone, total_count, max_count, json.dumps(grid_data))
                print(f"[DB] ✅ Inserted for {device_id}/{villa_id}/{zone}", flush=True)
    except Exception as e:
        print(f"[ERROR] ❌ {e}", flush=True)
        import traceback
        traceback.print_exc()


async def message_processor():
    print("[PROCESSOR] Started", flush=True)
    while running:
        try:
            topic, payload = await asyncio.wait_for(message_queue.get(), timeout=5.0)
            await process_message(topic, payload)
            message_queue.task_done()
        except asyncio.TimeoutError:
            pass
        except Exception as e:
            print(f"[PROCESSOR] Error: {e}", flush=True)


async def init_db():
    global db_pool
    print(f"[DB] Connecting to {DB_HOST}:{DB_PORT}...", flush=True)
    db_pool = await asyncpg.create_pool(
        host=DB_HOST, port=DB_PORT, user=DB_USER,
        password=DB_PASSWORD, database=DB_NAME,
        min_size=2, max_size=10
    )
    print("[DB] ✅ Pool created", flush=True)


async def main():
    global message_queue, running
    
    print("[INIT] 🚀 Starting MQTT Ingest Service", flush=True)
    print(f"[INIT] MQTT: {MQTT_HOST}:{MQTT_PORT}", flush=True)
    print(f"[INIT] DB: {DB_HOST}:{DB_PORT}/{DB_NAME}", flush=True)
    
    message_queue = asyncio.Queue()
    
    print("[INIT] Initializing DB...", flush=True)
    await init_db()
    
    print("[INIT] Starting message processor...", flush=True)
    processor_task = asyncio.create_task(message_processor())
    
    print("[MQTT] Connecting...", flush=True)
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    
    loop = asyncio.get_event_loop()
    await loop.run_in_executor(None, lambda: client.connect(MQTT_HOST, MQTT_PORT, 60))
    
    print("[MQTT] ✅ Connected, starting loop...", flush=True)
    client.loop_start()
    
    print("[INIT] ✅ Service running. Press Ctrl+C to stop.", flush=True)
    try:
        while running:
            await asyncio.sleep(1)
    except KeyboardInterrupt:
        print("[INIT] Shutting down...", flush=True)
        running = False
        client.loop_stop()
        client.disconnect()
        await processor_task


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("Exited", flush=True)
