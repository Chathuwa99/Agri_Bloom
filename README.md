# Agri_Bloom
Agri-Bloom is a low-cost device that monitors tomato seedlings from week 1 to week 8. It reads four sensors, diagnoses plant conditions on-device, and controls irrigation when needed.

## Main components

- `Agri_bloom.ino` - ESP32 firmware (sensors, diagnosis, irrigation, Firebase sync)
- `frontend/` - React dashboard (Firebase auth + realtime monitoring/control UI)

## Firebase data paths used

- `/AgriBloom/device_001/sensors`
- `/AgriBloom/device_001/status`
- `/AgriBloom/device_001/history`
- `/AgriBloom/device_001/alerts`
- `/AgriBloom/device_001/control`
- `/AgriBloom/device_001/settings`
