-- Enable TimescaleDB extension
CREATE EXTENSION IF NOT EXISTS timescaledb CASCADE;

-- Create schema
CREATE SCHEMA IF NOT EXISTS noisyless;

-- Users table
CREATE TABLE IF NOT EXISTS noisyless.users (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    email TEXT UNIQUE NOT NULL,
    password_hash TEXT,
    account_type TEXT CHECK (account_type IN ('perso', 'pro')) DEFAULT 'perso',
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- Auth tokens for magic links
CREATE TABLE IF NOT EXISTS noisyless.auth_tokens (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES noisyless.users(id) ON DELETE CASCADE,
    token_hash TEXT NOT NULL,
    expires_at TIMESTAMPTZ NOT NULL,
    used BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Devices table
CREATE TABLE IF NOT EXISTS noisyless.devices (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    device_id TEXT UNIQUE NOT NULL,
    device_type TEXT CHECK (device_type IN ('environmental', 'heatmap_tof', 'heatmap_radar')) NOT NULL,
    firmware_version TEXT,
    villa_id TEXT,
    calibration_json JSONB,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- User devices mapping
CREATE TABLE IF NOT EXISTS noisyless.user_devices (
    user_id UUID REFERENCES noisyless.users(id) ON DELETE CASCADE,
    device_id UUID REFERENCES noisyless.devices(id) ON DELETE CASCADE,
    granted_at TIMESTAMPTZ DEFAULT NOW(),
    PRIMARY KEY (user_id, device_id)
);

-- Measurements hypertable (TimescaleDB)
CREATE TABLE IF NOT EXISTS noisyless.measurements (
    time TIMESTAMPTZ NOT NULL,
    device_id TEXT NOT NULL,
    villa_id TEXT,
    zone TEXT,
    
    -- Environmental sensors (#089)
    temperature_c DOUBLE PRECISION,
    humidity_pct DOUBLE PRECISION,
    pressure_pa DOUBLE PRECISION,
    gas_ohms DOUBLE PRECISION,
    iaq DOUBLE PRECISION,
    co2eq_ppm DOUBLE PRECISION,
    bvoc_eq_ppm DOUBLE PRECISION,
    presence BOOLEAN,
    people_count INTEGER,
    target_distance_cm INTEGER,
    lux1_raw INTEGER,
    lux2_raw INTEGER,
    sound_mic1_pp INTEGER,
    sound_mic2_pp INTEGER,
    
    -- HeatMap sensors (#091)
    tof_grid JSONB,           -- 16x16 grid distances
    radar_targets JSONB,      -- [{x, y, z, v, confidence}]
    heatmap_cluster JSONB,    -- [{x_cm, y_cm, cells, confidence}]
    cluster_count INTEGER,
    
    -- System
    battery_mv INTEGER,
    rssi_dbm INTEGER,
    uptime_s BIGINT
);

-- Convert to hypertable
SELECT create_hypertable('noisyless.measurements', 'time', if_not_exists => TRUE);

-- Indexes
CREATE INDEX IF NOT EXISTS idx_measurements_device_time ON noisyless.measurements (device_id, time DESC);
CREATE INDEX IF NOT EXISTS idx_measurements_villa_time ON noisyless.measurements (villa_id, time DESC);
CREATE INDEX IF NOT EXISTS idx_measurements_zone ON noisyless.measurements (zone) WHERE zone IS NOT NULL;

-- Aggregated heatmaps (5-min windows)
CREATE TABLE IF NOT EXISTS noisyless.heatmap_agg_5min (
    window_start TIMESTAMPTZ NOT NULL,
    villa_id TEXT NOT NULL,
    zone TEXT,
    total_count INTEGER,
    max_count INTEGER,
    grid_data JSONB,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    PRIMARY KEY (window_start, villa_id, zone)
);

-- Alert thresholds per villa
CREATE TABLE IF NOT EXISTS noisyless.alert_thresholds (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    villa_id TEXT NOT NULL UNIQUE,
    threshold_count INTEGER DEFAULT 10,
    telegram_enabled BOOLEAN DEFAULT FALSE,
    email_enabled BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Device health logs
CREATE TABLE IF NOT EXISTS noisyless.device_health (
    time TIMESTAMPTZ DEFAULT NOW(),
    device_id TEXT NOT NULL,
    role TEXT,
    uptime_s BIGINT,
    battery_mv INTEGER,
    free_heap INTEGER,
    mesh_peers INTEGER,
    error_count INTEGER DEFAULT 0
);

SELECT create_hypertable('noisyless.device_health', 'time', if_not_exists => TRUE);
