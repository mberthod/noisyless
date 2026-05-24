"""
Heatmap Aggregator — NOISYLESS HeatMap #091
Aggregates ToF + Radar data into 5-min rolling heatmaps
"""

import os
import asyncio
import asyncpg
import json
from datetime import datetime, timedelta

DB_HOST = os.getenv("DB_HOST", "timescaledb")
DB_PORT = int(os.getenv("DB_PORT", "5432"))
DB_USER = os.getenv("DB_USER", "noisyless")
DB_PASSWORD = os.getenv("DB_PASSWORD", "noisyless_secret_password")
DB_NAME = os.getenv("DB_NAME", "noisyless")
AGG_WINDOW = int(os.getenv("AGG_WINDOW_SECONDS", "300"))

db_pool = None


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
    print("[DB] Heatmap agg pool created")


async def aggregate_heatmap():
    """Aggregate raw measurements into 5-min windows per villa/zone"""
    
    async with db_pool.acquire() as conn:
        # Get window boundaries
        now = datetime.utcnow()
        window_end = now.replace(second=0, microsecond=0)
        window_start = window_end - timedelta(seconds=AGG_WINDOW)
        
        # Fetch raw measurements in window
        rows = await conn.fetch(
            """
            SELECT villa_id, zone, cluster_count, heatmap_cluster
            FROM noisyless.measurements
            WHERE time >= $1 AND time < $2
            AND heatmap_cluster IS NOT NULL
            """,
            window_start,
            window_end,
        )
        
        if not rows:
            print(f"[AGG] No data for window {window_start} - {window_end}")
            return
        
        # Group by villa/zone
        aggregated = {}
        for row in rows:
            key = (row["villa_id"], row["zone"])
            if key not in aggregated:
                aggregated[key] = {
                    "total_count": 0,
                    "max_count": 0,
                    "grids": [],
                }
            
            count = row["cluster_count"] or 0
            aggregated[key]["total_count"] += count
            aggregated[key]["max_count"] = max(aggregated[key]["max_count"], count)
            
            if row["heatmap_cluster"]:
                aggregated[key]["grids"].append(row["heatmap_cluster"])
        
        # Insert aggregated data
        for (villa_id, zone), data in aggregated.items():
            # Merge grids (simple average for V1)
            merged_grid = merge_grids(data["grids"])
            
            await conn.execute(
                """
                INSERT INTO noisyless.heatmap_agg_5min 
                (window_start, villa_id, zone, total_count, max_count, grid_data)
                VALUES ($1, $2, $3, $4, $5, $6)
                ON CONFLICT (window_start, villa_id, zone) 
                DO UPDATE SET 
                    total_count = EXCLUDED.total_count,
                    max_count = EXCLUDED.max_count,
                    grid_data = EXCLUDED.grid_data
                """,
                window_start,
                villa_id,
                zone,
                data["total_count"],
                data["max_count"],
                json.dumps(merged_grid),
            )
            
            print(f"[AGG] Villa {villa_id}, zone {zone}: {data['total_count']} total, {data['max_count']} max")


def merge_grids(grids):
    """Simple grid merge: average overlapping cells"""
    if not grids:
        return None
    # V1: return latest grid
    return grids[-1]


async def run_aggregator():
    while True:
        try:
            await aggregate_heatmap()
        except Exception as e:
            print(f"[ERROR] Aggregation failed: {e}")
        
        # Run every 60s
        await asyncio.sleep(60)


async def main():
    await init_db()
    await run_aggregator()


if __name__ == "__main__":
    asyncio.run(main())
