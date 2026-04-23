# 🌱 Agri_Bloom — Smart Tomato Seedling Monitor
Agri-Bloom is a low-cost device that monitors tomato seedlings from week 1 to week 8. It reads four sensors, diagnoses plant conditions on-device, and controls irrigation when needed.

---

## ✨ Features

### 🔌 Hardware (ESP32)
- Soil moisture sensor (capacitive, analog) with smoothing
- DHT22 temperature & humidity sensor
- BH1750 light sensor (lux)
- 5V relay controlling water pump
- OLED display (128×64) with live data
- Dual buzzers (alerts + watering)
- On-device plant health diagnosis:
  - Healthy
  - Water Stress
  - Root Rot Risk
  - Heat Stress
  - Fungal Risk
  - Light Deficiency
  - Nutrient Deficiency
- Auto-watering with cooldown

---

### ⚙️ Backend (FastAPI)
- Single-file backend (`app.py`)
- No database server required
- Firebase Realtime DB integration (REST)
- Serves React frontend
- Provides APIs for:
  - Sensor data
  - Health status
  - Alerts
  - Settings
  - Pump control

---

### 💻 Dashboard
- Login system (persistent session)
- Live sensor data (updates every 5s)
- Health score + diagnosis
- 7-day trend charts
- Alerts system
- Manual watering control
- Settings management (synced with ESP32)

---


## 🚀 Quick Start

### 1️⃣ Pre-requisites
- Python 3.10+
- Firebase project (Realtime Database enabled)
- ESP32 + sensors
- Arduino IDE with:
  - Adafruit GFX
  - SSD1306
  - BH1750
  - DHT library
  - Firebase ESP32 Client

---

### 2️⃣ Configure Firebase

1. Create project
2. Enable Realtime Database
3. Set rules:

```json
{
  "rules": {
    ".read": true,
    ".write": true
  }
}
```

---


## 🚀 Run Backend

Follow these steps to start the backend server:

### 1. Install dependencies
```bash
pip install fastapi "uvicorn[standard]" requests
python -m uvicorn app:app --host 0.0.0.0 --port 8000
```
Open the dashboard
Go to: http://localhost:8000

---


### 🧠 **Enhanced Diagnosis Logic Table**


## 🧠 Diagnosis Logic

The ESP32 uses a rule-based system (or optional ML model) to classify plant health:

| Diagnosis | Condition | Recommended Action |
|-----------|-----------|-------------------|
| 💧 Water Stress | Soil moisture < 34% | Water immediately |
| 🥀 Root Rot Risk | Moisture ≥85% + High humidity + Low light | Stop watering, improve drainage |
| 🔥 Heat Stress | Temperature ≥32°C + Low humidity | Move to shade, increase air flow |
| 🍄 Fungal Risk | Wet soil + Humid + Low light | Reduce watering, increase ventilation |
| 💡 Light Deficiency | Light intensity < 1000 lux | Move to brighter location |
| 🌿 Nutrient Deficiency | Declining health score despite normal conditions | Apply balanced fertilizer |
| ✅ Healthy | All parameters within optimal range | Maintain current care |

**Note:** All thresholds can be adjusted from the dashboard settings and will sync to the ESP32 automatically.


---


## 🗺️ Future Improvements

Planned enhancements for the project:

| Feature | Status | Priority |
|---------|--------|----------|
| 🔄 Real-time Firebase streaming | Planned | High |
| 🔐 Firebase authentication | Planned | High |
| 📩 Email/Telegram/Discord notifications | Planned | Medium |
| 🌱 Multi-plant/device support | Planned | Medium |
| 📷 AI-based disease detection | Researching | Low |
| 📱 Mobile app development| Planned | Medium |
| ⚡ Reduce sync interval for faster response | In Progress | High |

### 🤝 Contributing
Feel free to open issues or submit PRs for any of these improvements!


---


## Main components

- `Agri_bloom.ino` - ESP32 firmware (sensors, diagnosis, irrigation, Firebase sync)
- `frontend/` - React dashboard (Firebase auth + realtime monitoring/control UI)


---


## Firebase data paths used

- `/AgriBloom/device_001/sensors`
- `/AgriBloom/device_001/status`
- `/AgriBloom/device_001/history`
- `/AgriBloom/device_001/alerts`
- `/AgriBloom/device_001/control`
- `/AgriBloom/device_001/settings`
