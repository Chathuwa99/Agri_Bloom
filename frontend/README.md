# Agri Bloom Frontend

This dashboard now uses **Firebase Authentication** and **Firebase Realtime Database** directly (no mock-data dependency for core app features).

## What is live now

- Email/password authentication (sign in + account creation)
- Live sensor/status stream from:
  - `/AgriBloom/device_001/sensors`
  - `/AgriBloom/device_001/status`
- Historical chart data from:
  - `/AgriBloom/device_001/history`
- Active alert list and acknowledge action from:
  - `/AgriBloom/device_001/alerts`
- Remote control commands for watering/automation from:
  - `/AgriBloom/device_001/control`
- Threshold + notification settings:
  - `/AgriBloom/device_001/settings`

## Local development

```bash
npm install
npm run dev
```

## Firebase Console setup required

1. Enable **Authentication > Sign-in method > Email/Password**.
2. In **Realtime Database**, create database in production or test mode.
3. Add rules that allow authenticated app users and device paths.

### Example Realtime Database rules

```json
{
  "rules": {
    "AgriBloom": {
      "device_001": {
        ".read": "auth != null",
        "control": {
          ".write": "auth != null"
        },
        "settings": {
          ".write": "auth != null"
        },
        "alerts": {
          "$alertId": {
            ".write": "auth != null"
          }
        },
        ".write": "auth != null"
      }
    }
  }
}
```

Adjust rules as needed if your ESP32 writes without Firebase user auth.
