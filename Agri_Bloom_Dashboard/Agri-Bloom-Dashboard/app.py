"""
Agri_Bloom — single-file FastAPI backend.

Bridges the React dashboard to the ESP32 via Firebase Realtime Database.
Matches the Firebase data layout written by the ESP32 sketch.

Firebase paths used:
    /AgriBloom/device_001
        /sensors/temperature      (float)
        /sensors/humidity         (float)
        /sensors/moisturePct      (float)
        /sensors/lux              (float)
        /status/diagnosis         (string)
        /status/recommendation    (string)
        /status/pumpActive        (bool)
        /status/healthScore       (int)
        /control/autoWatering     (bool)
        /control/pumpDurationSeconds (int)
        /control/manualWaterNow   (bool)         <- dashboard writes true
        /settings/thresholds/soilMin .. lightMax (float)
        /settings/notificationsEnabled (bool)
        /history/<id>/{temperature, humidity, moisturePct, lux, health,
                       diagnosis, pumpActive, createdAt}
        /alerts/<id>/{type, sensor, message, value, time, createdAt,
                      acknowledged}
"""
import os
import time
from datetime import datetime, timedelta

import requests
from fastapi import FastAPI, HTTPException
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel


# ─── Setup ────────────────────────────────────────────────────────────────────

app = FastAPI(title="Agri_Bloom API")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DIST_DIR = os.path.join(BASE_DIR, "dist", "public")

FIREBASE_URL = os.environ.get(
    "FIREBASE_URL",
    "https://agri-bloom-59b2a-default-rtdb.firebaseio.com",
).rstrip("/")

FIREBASE_AUTH = os.environ.get("FIREBASE_AUTH", "").strip()

DEVICE = os.environ.get("DEVICE_ID", "device_001")
ROOT = f"AgriBloom/{DEVICE}"


# ─── Firebase REST helpers ────────────────────────────────────────────────────

def _fb_url(path: str) -> str:
    url = f"{FIREBASE_URL}/{path}.json"
    if FIREBASE_AUTH:
        url += f"?auth={FIREBASE_AUTH}"
    return url


def fb_get(path: str):
    try:
        r = requests.get(_fb_url(path), timeout=5)
        r.raise_for_status()
        return r.json()
    except Exception as e:
        print(f"[firebase] GET {path} failed: {e}")
        return None


def fb_put(path: str, value):
    try:
        r = requests.put(_fb_url(path), json=value, timeout=5)
        r.raise_for_status()
        return r.json()
    except Exception as e:
        print(f"[firebase] PUT {path} failed: {e}")
        return None


def fb_patch(path: str, value: dict):
    try:
        r = requests.patch(_fb_url(path), json=value, timeout=5)
        r.raise_for_status()
        return r.json()
    except Exception as e:
        print(f"[firebase] PATCH {path} failed: {e}")
        return None


# ─── Pydantic models ──────────────────────────────────────────────────────────

class WaterCommand(BaseModel):
    durationSeconds: int


class Settings(BaseModel):
    soilMoistureMin: float
    soilMoistureMax: float
    temperatureMin: float
    temperatureMax: float
    humidityMin: float
    humidityMax: float
    lightMin: float
    lightMax: float
    autoWatering: bool
    wateringDuration: int
    notificationsEnabled: bool


# ─── API: Sensor data ─────────────────────────────────────────────────────────

@app.get("/api/sensor/current")
async def get_sensor_current():
    sensors = fb_get(f"{ROOT}/sensors") or {}
    status = fb_get(f"{ROOT}/status") or {}
    return {
        "soilMoisture": float(sensors.get("moisturePct", 0) or 0),
        "temperature":  float(sensors.get("temperature", 0) or 0),
        "humidity":     float(sensors.get("humidity", 0) or 0),
        "lightIntensity": float(sensors.get("lux", 0) or 0),
        "diagnosis":    status.get("diagnosis", "Unknown"),
        "recommendation": status.get("recommendation", ""),
        "pumpActive":   bool(status.get("pumpActive", False)),
        "healthScore":  int(status.get("healthScore", 0) or 0),
        "timestamp":    datetime.now().isoformat(),
    }


@app.get("/api/sensor/health")
async def get_plant_health():
    status = fb_get(f"{ROOT}/status") or {}
    score = int(status.get("healthScore", 0) or 0)
    diagnosis = status.get("diagnosis", "Unknown")
    recommendation = status.get("recommendation", "Monitor plant conditions")

    if score >= 85:
        label = "Excellent"
    elif score >= 70:
        label = "Good"
    elif score >= 55:
        label = "Fair"
    elif score >= 40:
        label = "Poor"
    else:
        label = "Critical"

    return {
        "score": score,
        "status": label,
        "condition": diagnosis,
        "recommendation": recommendation,
    }


@app.get("/api/sensor/history")
async def get_sensor_history():
    """
    Reads /AgriBloom/<device>/history (auto-id → snapshot) and groups the
    snapshots into the last 7 days. Returns daily averages for the chart.
    """
    raw = fb_get(f"{ROOT}/history") or {}
    if not isinstance(raw, dict):
        return []

    today = datetime.now().date()
    buckets = {(today - timedelta(days=i)): [] for i in range(6, -1, -1)}

    for entry in raw.values():
        if not isinstance(entry, dict):
            continue
        created = entry.get("createdAt")
        if isinstance(created, (int, float)):
            # ESP32 uses millis() since boot — that won't yield a real date.
            # Fall back to "today" so the snapshot is still visible.
            try:
                # Treat values > 1e12 as ms epoch, between 1e9-1e12 as s epoch.
                if created > 1e12:
                    d = datetime.fromtimestamp(created / 1000).date()
                elif created > 1e9:
                    d = datetime.fromtimestamp(created).date()
                else:
                    d = today
            except Exception:
                d = today
        else:
            d = today

        if d not in buckets:
            continue

        buckets[d].append({
            "soilMoisture": float(entry.get("moisturePct", 0) or 0),
            "temperature":  float(entry.get("temperature", 0) or 0),
            "humidity":     float(entry.get("humidity", 0) or 0),
            "lightIntensity": float(entry.get("lux", 0) or 0),
            "healthScore":  int(entry.get("health", 0) or 0),
        })

    out = []
    for day, items in buckets.items():
        if items:
            soil  = sum(x["soilMoisture"]    for x in items) / len(items)
            temp  = sum(x["temperature"]     for x in items) / len(items)
            hum   = sum(x["humidity"]        for x in items) / len(items)
            light = sum(x["lightIntensity"]  for x in items) / len(items)
            health = round(sum(x["healthScore"] for x in items) / len(items))
        else:
            soil = temp = hum = light = 0
            health = 0
        out.append({
            "date": day.strftime("%b %d"),
            "soilMoisture":   round(soil, 1),
            "temperature":    round(temp, 1),
            "humidity":       round(hum, 1),
            "lightIntensity": round(light),
            "healthScore":    health,
        })
    return out


# ─── API: Alerts ──────────────────────────────────────────────────────────────

@app.get("/api/alerts")
async def get_alerts():
    raw = fb_get(f"{ROOT}/alerts") or {}
    if not isinstance(raw, dict):
        return []

    alerts = []
    for fb_id, entry in raw.items():
        if not isinstance(entry, dict):
            continue
        created = entry.get("createdAt")
        if isinstance(created, (int, float)):
            ts = datetime.now().isoformat()  # ESP32 uses millis(); use now()
        else:
            ts = datetime.now().isoformat()
        alerts.append({
            "id": fb_id,
            "type": entry.get("type", "info"),
            "message": entry.get("message", ""),
            "sensor": entry.get("sensor", ""),
            "value": entry.get("value", ""),
            "threshold": 0,
            "acknowledged": bool(entry.get("acknowledged", False)),
            "timestamp": ts,
        })
    # newest first
    alerts.sort(key=lambda a: a["id"], reverse=True)
    return alerts


@app.post("/api/alerts/{alert_id}/acknowledge")
async def acknowledge_alert(alert_id: str):
    result = fb_patch(f"{ROOT}/alerts/{alert_id}", {"acknowledged": True})
    if result is None:
        raise HTTPException(status_code=502, detail="Could not reach Firebase")
    return {"id": alert_id, "acknowledged": True}


# ─── API: Control ─────────────────────────────────────────────────────────────

@app.post("/api/control/water")
async def trigger_watering(cmd: WaterCommand):
    """
    Sets manualWaterNow=true (and optionally updates pumpDurationSeconds) so
    the ESP32 picks it up on its next sync.  ESP32 clamps duration to 1–12s.
    """
    duration = max(1, min(12, int(cmd.durationSeconds)))
    fb_put(f"{ROOT}/control/pumpDurationSeconds", duration)
    result = fb_put(f"{ROOT}/control/manualWaterNow", True)

    if result is None:
        raise HTTPException(status_code=502, detail="Could not reach Firebase")

    return {
        "success": True,
        "message": f"Pump command sent. ESP32 will run pump for {duration}s.",
        "timestamp": datetime.now().isoformat(),
    }


# ─── API: Settings ────────────────────────────────────────────────────────────

# Map between dashboard names and Firebase paths
SETTINGS_PATH_MAP = {
    "soilMoistureMin":      f"{ROOT}/settings/thresholds/soilMin",
    "soilMoistureMax":      f"{ROOT}/settings/thresholds/soilMax",
    "temperatureMin":       f"{ROOT}/settings/thresholds/tempMin",
    "temperatureMax":       f"{ROOT}/settings/thresholds/tempMax",
    "humidityMin":          f"{ROOT}/settings/thresholds/humidityMin",
    "humidityMax":          f"{ROOT}/settings/thresholds/humidityMax",
    "lightMin":             f"{ROOT}/settings/thresholds/lightMin",
    "lightMax":             f"{ROOT}/settings/thresholds/lightMax",
    "autoWatering":         f"{ROOT}/control/autoWatering",
    "wateringDuration":     f"{ROOT}/control/pumpDurationSeconds",
    "notificationsEnabled": f"{ROOT}/settings/notificationsEnabled",
}

DEFAULTS = {
    "soilMoistureMin": 60.0, "soilMoistureMax": 70.0,
    "temperatureMin": 18.0,  "temperatureMax": 27.0,
    "humidityMin": 50.0,     "humidityMax": 80.0,
    "lightMin": 5000.0,      "lightMax": 5500.0,
    "autoWatering": True,    "wateringDuration": 5,
    "notificationsEnabled": True,
}


@app.get("/api/settings")
async def get_settings():
    out = {}
    for key, path in SETTINGS_PATH_MAP.items():
        val = fb_get(path)
        if val is None:
            val = DEFAULTS[key]
        out[key] = val
    return out


@app.put("/api/settings")
async def update_settings(body: Settings):
    payload = body.model_dump()
    thresholds = {
        "soilMin":     float(payload["soilMoistureMin"]),
        "soilMax":     float(payload["soilMoistureMax"]),
        "tempMin":     float(payload["temperatureMin"]),
        "tempMax":     float(payload["temperatureMax"]),
        "humidityMin": float(payload["humidityMin"]),
        "humidityMax": float(payload["humidityMax"]),
        "lightMin":    float(payload["lightMin"]),
        "lightMax":    float(payload["lightMax"]),
    }
    fb_patch(f"{ROOT}/settings/thresholds", thresholds)
    fb_put(f"{ROOT}/control/autoWatering", bool(payload["autoWatering"]))
    duration = max(1, min(12, int(payload["wateringDuration"])))
    fb_put(f"{ROOT}/control/pumpDurationSeconds", duration)
    fb_put(f"{ROOT}/settings/notificationsEnabled",
           bool(payload["notificationsEnabled"]))
    return payload


# ─── React frontend (built by vite into dist/public) ──────────────────────────

if os.path.isdir(DIST_DIR):
    assets_dir = os.path.join(DIST_DIR, "assets")
    if os.path.isdir(assets_dir):
        app.mount("/assets", StaticFiles(directory=assets_dir), name="assets")

    @app.get("/{full_path:path}")
    async def spa_fallback(full_path: str):
        if full_path:
            candidate = os.path.join(DIST_DIR, full_path)
            if os.path.isfile(candidate):
                return FileResponse(candidate)
        return FileResponse(os.path.join(DIST_DIR, "index.html"))


# ─── Entry point ──────────────────────────────────────────────────────────────

if __name__ == "__main__":
    import uvicorn
    port = int(os.environ.get("PORT", "8000"))
    uvicorn.run(app, host="0.0.0.0", port=port)
