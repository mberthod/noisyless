"""NOISYLESS — FastAPI Backend"""
import os, sys, asyncpg
from contextlib import asynccontextmanager
from fastapi import FastAPI, Depends, HTTPException
from fastapi.middleware.cors import CORSMiddleware

API_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, API_DIR)

DB_HOST = os.getenv("DB_HOST", "timescaledb")
DB_PORT = int(os.getenv("DB_PORT", "5432"))
DB_USER = os.getenv("DB_USER", "noisyless")
DB_PASSWORD = os.getenv("DB_PASSWORD", "noisyless_secret_password")
DB_NAME = os.getenv("DB_NAME", "noisyless")

db_pool = None

@asynccontextmanager
async def lifespan(app: FastAPI):
    global db_pool
    db_pool = await asyncpg.create_pool(host=DB_HOST, port=DB_PORT, user=DB_USER, password=DB_PASSWORD, database=DB_NAME, min_size=2, max_size=10)
    print("[API] Database pool created")
    yield
    await db_pool.close()

app = FastAPI(title="NOISYLESS API", lifespan=lifespan)
app.add_middleware(CORSMiddleware, allow_origins=["https://platform.noisyless.com", "https://noisyless.com", "http://localhost:3000", "http://localhost:5173"], allow_credentials=True, allow_methods=["*"], allow_headers=["*"])

from auth import router as auth_router
from devices import router as devices_router
from shop import router as shop_router
from pilot import router as pilot_router
from customer import router as customer_router

app.include_router(auth_router, prefix="/api")
app.include_router(devices_router, prefix="/api", tags=["Devices"])
app.include_router(shop_router)
app.include_router(pilot_router)
app.include_router(customer_router)

@app.get("/health")
async def health():
    return {"status": "ok", "service": "noisyless-api"}

@app.get("/api/contact")
async def contact_get():
    return {"message": "Use POST /auth/contact for contact form"}

from admin import router as admin_router
app.include_router(admin_router)
