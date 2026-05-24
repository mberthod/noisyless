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
    if rc == 0:
        print(f"[MQTT] Connected to {MQTT_HOST}:{MQTT_PORT}")
        client.subscribe(MQTT_TOPIC)
        print(f"[MQTT] Subscribed to {MQTT_TOPIC}")
    else:
        print(f"[MQTT] Connection failed: {rc}")


def on_message(client, userdata, msg):
    asyncio.create_task(process_message(msg.topic, msg.payload))


async def process_message(topic: str, payload: bytes):
    try:
        data = json.loads(payload.decode("utf-8"))
        print(f"[MQTT] Received: {topic}")
        
        # Extract device_id from topic: noisyless/{device_id}/heatmap
        parts = topic.split("/")
        device_id = parts[1] if len(parts) > 1 else "unknown"
        villa_id = data.get("villa_id", "unassigned")
        
        # Insert measurement
        await db_pool.execute(
            """
            INSERT INTO noisyless.measurements (
                time, device_id, villa_id, 
                cluster_count, heatmap_cluster, battery_mv
            ) VALUES ($1, $2, $3, $4, $5, $6)
            """,
            datetime.utcnow(),
            device_id,
            villa_id,
            data.get("total_count", 0),
            json.dumps(data.get("heatmap", {})),
            data.get("battery_mv"),
        )
        print(f"[DB] Inserted measurement for {device_id}")
        
    except Exception as e:
        print(f"[ERROR] Failed to process message: {e}")


async def init_db():
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
    print("[DB] Connection pool created")


async def main():
    await init_db()
    
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    
    client.connect(MQTT_HOST, MQTT_PORT, 60)
    client.loop_forever()


if __name__ == "__main__":
    asyncio.run(main())
