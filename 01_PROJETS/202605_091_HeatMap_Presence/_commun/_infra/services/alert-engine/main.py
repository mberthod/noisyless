"""
Alert Engine — NOISYLESS HeatMap #091
Monitors thresholds and sends Telegram/email alerts
"""

import os
import asyncio
import asyncpg
import requests
from datetime import datetime, timedelta

DB_HOST = os.getenv("DB_HOST", "timescaledb")
DB_PORT = int(os.getenv("DB_PORT", "5432"))
DB_USER = os.getenv("DB_USER", "noisyless")
DB_PASSWORD = os.getenv("DB_PASSWORD", "noisyless_secret_password")
DB_NAME = os.getenv("DB_NAME", "noisyless")

TELEGRAM_TOKEN = os.getenv("TELEGRAM_BOT_TOKEN", "")
TELEGRAM_CHAT_ID = os.getenv("TELEGRAM_CHAT_ID", "")

db_pool = None
alerted_windows = set()  # Track already alerted windows


async def init_db():
    global db_pool
    db_pool = await asyncpg.create_pool(
        host=DB_HOST,
        port=DB_PORT,
        user=DB_USER,
        password=DB_PASSWORD,
        database=DB_NAME,
        min_size=2,
        max_size=5,
    )
    print("[DB] Alert engine pool created")


async def check_thresholds():
    """Check aggregated heatmaps against thresholds"""
    
    async with db_pool.acquire() as conn:
        # Get active thresholds
        thresholds = await conn.fetch(
            "SELECT villa_id, threshold_count FROM noisyless.alert_thresholds"
        )
        
        for threshold in thresholds:
            villa_id = threshold["villa_id"]
            max_count = threshold["threshold_count"]
            
            # Get latest 5-min aggregation
            row = await conn.fetchrow(
                """
                SELECT window_start, total_count, max_count
                FROM noisyless.heatmap_agg_5min
                WHERE villa_id = $1
                ORDER BY window_start DESC
                LIMIT 1
                """,
                villa_id,
            )
            
            if not row:
                continue
            
            window_key = f"{villa_id}_{row['window_start']}"
            
            # Check if threshold breached
            if row["max_count"] > max_count and window_key not in alerted_windows:
                print(f"[ALERT] Villa {villa_id}: {row['max_count']} > {max_count}")
                await send_alert(villa_id, row["max_count"], row["window_start"])
                alerted_windows.add(window_key)
                
                # Clean old alerts (keep only last hour)
                cutoff = datetime.utcnow() - timedelta(hours=1)
                alerted_windows.clear()


async def send_alert(villa_id: str, count: int, window_start: datetime):
    """Send Telegram alert"""
    
    if not TELEGRAM_TOKEN or not TELEGRAM_CHAT_ID:
        print("[ALERT] Telegram not configured")
        return
    
    message = f"""
🚨 **ALERT - Suroccupation Détectée**

📍 Villa: {villa_id}
👥 Personnes: {count}
⏰ Heure: {window_start.strftime('%Y-%m-%d %H:%M')}

"""
    
    url = f"https://api.telegram.org/bot{TELEGRAM_TOKEN}/sendMessage"
    payload = {
        "chat_id": TELEGRAM_CHAT_ID,
        "text": message,
        "parse_mode": "Markdown",
    }
    
    try:
        response = requests.post(url, json=payload, timeout=10)
        if response.ok:
            print(f"[ALERT] Telegram sent for {villa_id}")
        else:
            print(f"[ALERT] Telegram failed: {response.text}")
    except Exception as e:
        print(f"[ALERT] Error sending Telegram: {e}")


async def run_alert_engine():
    while True:
        try:
            await check_thresholds()
        except Exception as e:
            print(f"[ERROR] Alert check failed: {e}")
        
        # Check every 30s
        await asyncio.sleep(30)


async def main():
    await init_db()
    await run_alert_engine()


if __name__ == "__main__":
    asyncio.run(main())
