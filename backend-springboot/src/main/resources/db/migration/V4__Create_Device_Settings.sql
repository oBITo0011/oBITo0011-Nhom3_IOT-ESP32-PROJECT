CREATE TABLE device_settings (
    id UUID PRIMARY KEY,
    device_id VARCHAR(255) NOT NULL UNIQUE,
    temperature_threshold DOUBLE PRECISION NOT NULL DEFAULT 32.0,
    soil_moisture_min DOUBLE PRECISION NOT NULL DEFAULT 30.0,
    soil_moisture_max DOUBLE PRECISION NOT NULL DEFAULT 80.0
);
