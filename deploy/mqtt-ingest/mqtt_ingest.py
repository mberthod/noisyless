"""
NOISYLESS MQTT Ingest Service
Subscribes to MQTT broker, parses ESP32 sensor JSON, inserts into TimescaleDB.
"""

import json
import math
import os
import signal
import sys
import time
from datetime import datetime, timezone

import paho.mqtt.client as mqtt
import psycopg2
from psycopg2.extras import execute_values

sys.stdout.reconfigure(line_buffering=True)
sys.stderr.reconfigure(line_buffering=True)

# ── Config ──────────────────────────────────────────────
MQTT_HOST = os.getenv("MQTT_HOST", "mosquitto")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_USER = os.getenv("MQTT_USER", "esp32")
MQTT_PASS = os.getenv("MQTT_PASS", "")
MQTT_TOPIC = os.getenv("MQTT_TOPIC", "esp32/noisyless/+/datas")

DATABASE_URL = os.getenv("DATABASE_URL", "")

# ADC → physical conversion constants
# Microphone: peak-to-peak ADC (12-bit, 0-4095) → approximate dB SPL
# Reference: silence ~50 pp, loud ~2000 pp. Linear approx for MVP.
MIC_REF_PP = 1.0       # reference pp value (avoid log(0))
MIC_DB_OFFSET = 30.0   # dB offset (ambient floor)
MIC_DB_SCALE = 20.0    # dB per decade

# Lux: raw ADC → approximate lux (linear, sensor-dependent)
LUX_SCALE = 0.25       # lux per ADC count (rough calibration)

running = True


def mic_pp_to_db(pp: int) -> float:
    """Convert peak-to-peak ADC value to approximate dB SPL."""
    if pp <= 0:
        return MIC_DB_OFFSET
    return MIC_DB_OFFSET + MIC_DB_SCALE * math.log10(max(pp, MIC_REF_PP))


def adc_to_lux(raw: int) -> float:
    """Convert raw ADC reading to approximate lux."""
    return round(raw * LUX_SCALE, 1)


def get_db_conn():
    """Create a new psycopg2 connection."""
    for attempt in range(5):
        try:
            conn = psycopg2.connect(DATABASE_URL)
            conn.autocommit = True
            return conn
        except Exception as e:
            print(f"[DB] Connection attempt {attempt+1}/5 failed: {e}")
            time.sleep(3)
    raise RuntimeError("Cannot connect to database after 5 attempts")


def insert_measurement(conn, data: dict):
    """Insert a single measurement row into TimescaleDB."""
    device_id = data.get("device_id", "unknown")
    now = datetime.now(timezone.utc)

    mic1_pp = data.get("sound_mic1_pp", 0)
    mic2_pp = data.get("sound_mic2_pp", 0)
    db_avg = mic_pp_to_db(mic1_pp)
    db_peak = mic_pp_to_db(max(mic1_pp, mic2_pp))

    lux1 = adc_to_lux(data.get("lux1_raw", 0))
    lux2 = adc_to_lux(data.get("lux2_raw", 0))

    with conn.cursor() as cur:
        cur.execute("""
            INSERT INTO measurements (time, device_id, temp, humidity, gas_resistance, iaq,
                                      presence, distance_cm, db_avg, db_peak, lux1, lux2)
            VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
            ON CONFLICT (time, device_id) DO NOTHING
        """, (
            now,
            device_id,
            data.get("temperature_c"),
            data.get("humidity_pct"),
            data.get("gas_ohms"),
            int(data.get("iaq", 0)) if data.get("iaq") is not None else None,
            data.get("presence"),
            data.get("target_distance_cm"),
            round(db_avg, 1),
            round(db_peak, 1),
            lux1,
            lux2,
        ))

        # Update device last_seen + firmware version
        cur.execute("""
            INSERT INTO devices (device_id, name, firmware_version, last_seen)
            VALUES (%s, %s, %s, %s)
            ON CONFLICT (device_id) DO UPDATE
                SET last_seen = EXCLUDED.last_seen,
                    firmware_version = EXCLUDED.firmware_version
        """, (
            device_id,
            data.get("product", device_id),
            data.get("fw_version"),
            now,
        ))

    print(f"[INGEST] {device_id} | temp={data.get('temperature_c')}C "
          f"iaq={data.get('iaq')} presence={data.get('presence')} "
          f"db_avg={db_avg:.1f} lux1={lux1}")


# ── MQTT callbacks ──────────────────────────────────────
db_conn = None


def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print(f"[MQTT] Connected to {MQTT_HOST}:{MQTT_PORT}")
        client.subscribe(MQTT_TOPIC)
        print(f"[MQTT] Subscribed to {MQTT_TOPIC}")
    else:
        print(f"[MQTT] Connect failed rc={rc}")


def on_message(client, userdata, msg):
    global db_conn
    try:
        payload = json.loads(msg.payload.decode("utf-8"))
    except (json.JSONDecodeError, UnicodeDecodeError) as e:
        print(f"[MQTT] Bad payload on {msg.topic}: {e}")
        return

    try:
        if db_conn is None or db_conn.closed:
            db_conn = get_db_conn()
        insert_measurement(db_conn, payload)
    except psycopg2.OperationalError as e:
        print(f"[DB] Connection lost, reconnecting: {e}")
        db_conn = None
    except Exception as e:
        print(f"[INGEST] Error: {e}")


def on_disconnect(client, userdata, rc, properties=None):
    print(f"[MQTT] Disconnected rc={rc}, will auto-reconnect")


def shutdown(sig, frame):
    global running
    print(f"[INGEST] Received signal {sig}, shutting down...")
    running = False


def main():
    global db_conn, running

    signal.signal(signal.SIGTERM, shutdown)
    signal.signal(signal.SIGINT, shutdown)

    if not DATABASE_URL:
        print("[FATAL] DATABASE_URL not set")
        sys.exit(1)

    # Test DB connection at startup
    db_conn = get_db_conn()
    print(f"[DB] Connected to TimescaleDB")

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="noisyless-ingest")
    client.username_pw_set(MQTT_USER, MQTT_PASS)
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    client.reconnect_delay_set(min_delay=1, max_delay=30)

    print(f"[MQTT] Connecting to {MQTT_HOST}:{MQTT_PORT} as {MQTT_USER}...")
    client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
    client.loop_start()

    while running:
        time.sleep(1)

    client.loop_stop()
    client.disconnect()
    if db_conn and not db_conn.closed:
        db_conn.close()
    print("[INGEST] Clean shutdown.")


if __name__ == "__main__":
    main()
