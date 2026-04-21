#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BH1750.h>
#include <DHT.h>
#include <FirebaseESP32.h>
#include <WiFi.h>
#include <Wire.h>


#define WIFI_SSID "ESP32"
#define WIFI_PASSWORD "AgriBloom123#"
#define FIREBASE_HOST "agri-bloom-59b2a-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "AIzaSyDo20_bhC1QcwWB86yiZA2mjvo9uDw1aC8"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

#define DHTPIN 18
#define DHTTYPE DHT22
#define SOIL_PIN 1
#define PUMP_PIN 5

#define RISK_BUZZER_PIN 13
#define WATER_BUZZER_PIN 14

#define I2C_SDA 8
#define I2C_SCL 9

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

#define SOIL_DRY_VALUE 3350
#define SOIL_WET_VALUE 1257
#define SOIL_SAMPLES 30

// -------------------- TIMERS --------------------
const unsigned long STARTUP_DURATION = 5000;   // 5 sec startup screen
const unsigned long SCREEN_DURATION = 5000;    // 5 sec per OLED screen
const unsigned long SENSOR_INTERVAL = 30000;   // 30 sec sensor read/print
const unsigned long WATERING_DURATION = 5000;  // 5 sec motor ON
const unsigned long WATERING_COOLDOWN = 55000; // 55 sec wait after watering

DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

String diagnosis = "Unknown";
String recommendation = "Monitor plant conditions";

String lastDiagnosis = "";
bool lastPumpOn = false;

unsigned long screenTimer = 0;
unsigned long sensorTimer = 0;
unsigned long wateringStartTime = 0;
unsigned long cooldownStartTime = 0;

int currentScreen = 1;
bool startupDone = false;

bool wateringActive = false;
bool cooldownActive = false;

// latest sensor values
float latestTemp = NAN;
float latestHum = NAN;
float latestMoisturePct = 0.0;
float latestLux = 0.0;
int latestRawSoil = 0;
bool latestPumpOn = false;

const String DEVICE_PATH = "/AgriBloom/device_001";

unsigned long configuredWateringDuration = WATERING_DURATION;
bool autoWateringEnabled = true;
bool manualWaterRequested = false;

float thresholdSoilMin = 60.0;
float thresholdSoilMax = 70.0;
float thresholdTempMin = 18.0;
float thresholdTempMax = 27.0;
float thresholdHumidityMin = 50.0;
float thresholdHumidityMax = 80.0;
float thresholdLightMin = 5000.0;
float thresholdLightMax = 5500.0;

unsigned long lastSoilAlertAt = 0;
unsigned long lastTempAlertAt = 0;
unsigned long lastHumidityAlertAt = 0;
unsigned long lastLightAlertAt = 0;
unsigned long lastDiagnosisAlertAt = 0;
const unsigned long ALERT_THROTTLE = 120000;

// -------------------- HELPERS --------------------

int readAveragedAnalog(int pin, int samples) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(10);
  }
  return sum / samples;
}

float getMoisturePct(int rawSoil) {
  float moisturePct =
      ((float)(SOIL_DRY_VALUE - rawSoil) / (SOIL_DRY_VALUE - SOIL_WET_VALUE)) *
      100.0;

  if (moisturePct < 0)
    moisturePct = 0;
  if (moisturePct > 100)
    moisturePct = 100;

  return moisturePct;
}

float smoothMoisture(float currentValue) {
  static float lastValue = currentValue;
  float smoothed = (lastValue * 0.7) + (currentValue * 0.3);
  lastValue = smoothed;
  return smoothed;
}

// -------------------- DIAGNOSIS --------------------

String diagnosePlant(float moisturePct, float lux, float temp, float hum) {
  if (moisturePct >= 0.0 && moisturePct <= 34.0) {
    return "Water Stress";
  }

  if (moisturePct >= 85.0 && moisturePct <= 100.0 && lux >= 500.0 &&
      lux <= 2000.0 && temp >= 18.0 && temp <= 26.0 && hum >= 80.0 &&
      hum <= 95.0) {
    return "Root Rot Risk";
  }

  if (moisturePct >= 30.0 && moisturePct <= 55.0 && temp >= 32.0 &&
      temp <= 42.0 && hum >= 20.0 && hum <= 40.0) {
    return "Heat Stress";
  }

  if (moisturePct >= 60.0 && moisturePct <= 80.0 && lux >= 300.0 &&
      lux <= 1500.0 && temp >= 18.0 && temp <= 25.0 && hum >= 85.0 &&
      hum <= 100.0) {
    return "Fungal Risk";
  }

  if (moisturePct >= 50.0 && moisturePct <= 70.0 && lux >= 100.0 &&
      lux <= 999.0 && temp >= 15.0 && temp <= 25.0) {
    return "Light Deficiency";
  }

  if (moisturePct >= 60.0 && moisturePct <= 70.0 && lux >= 3000.0 &&
      lux <= 5000.0 && temp >= 18.0 && temp <= 27.0 && hum >= 60.0 &&
      hum <= 80.0) {
    return "Healthy";
  }

  if (moisturePct >= 40.0 && moisturePct <= 70.0 && lux >= 500.0 &&
      lux <= 4500.0 && temp >= 18.0 && temp <= 27.0 && hum >= 40.0 &&
      hum <= 85.0) {
    return "Nutrient Deficiency";
  }

  return "Unknown";
}

bool isRiskDiagnosis(String diagnosis) {
  return (diagnosis == "Water Stress" || diagnosis == "Root Rot Risk" ||
          diagnosis == "Heat Stress" || diagnosis == "Fungal Risk");
}

String getRecommendation(String diagnosis, bool wateringNow, bool cooldownNow) {
  if (wateringNow) {
    return "Watering now";
  }

  if (cooldownNow && diagnosis == "Water Stress") {
    return "Waiting to recheck";
  }

  if (diagnosis == "Water Stress") {
    return "Water the plant now";
  } else if (diagnosis == "Root Rot Risk") {
    return "Stop watering";
  } else if (diagnosis == "Heat Stress") {
    return "Cool the plant";
  } else if (diagnosis == "Fungal Risk") {
    return "Reduce moisture";
  } else if (diagnosis == "Light Deficiency") {
    return "Increase light";
  } else if (diagnosis == "Nutrient Deficiency") {
    return "Check fertilizer";
  } else if (diagnosis == "Healthy") {
    return "Plant is stable";
  }

  return "Monitor conditions";
}

int computeHealthScore(String currentDiagnosis, float moisturePct, float temp, float hum,
                       float lux) {
  int score = 100;

  if (currentDiagnosis == "Water Stress")
    score -= 28;
  else if (currentDiagnosis == "Root Rot Risk")
    score -= 30;
  else if (currentDiagnosis == "Heat Stress")
    score -= 24;
  else if (currentDiagnosis == "Fungal Risk")
    score -= 26;
  else if (currentDiagnosis == "Light Deficiency")
    score -= 16;
  else if (currentDiagnosis == "Nutrient Deficiency")
    score -= 14;
  else if (currentDiagnosis == "Unknown")
    score -= 20;

  if (moisturePct < thresholdSoilMin)
    score -= 6;
  if (moisturePct > thresholdSoilMax)
    score -= 6;
  if (temp < thresholdTempMin)
    score -= 5;
  if (temp > thresholdTempMax)
    score -= 8;
  if (hum < thresholdHumidityMin || hum > thresholdHumidityMax)
    score -= 5;
  if (lux < thresholdLightMin || lux > thresholdLightMax)
    score -= 5;

  if (score < 0)
    score = 0;
  if (score > 100)
    score = 100;

  return score;
}

bool shouldSendAlert(unsigned long &lastSentAt, unsigned long now) {
  if (now - lastSentAt < ALERT_THROTTLE) {
    return false;
  }

  lastSentAt = now;
  return true;
}

void pushAlertToFirebase(String type, String sensor, String message, String value,
                         unsigned long now) {
  String alertId = String(now) + "_" + String(random(100, 999));
  String basePath = DEVICE_PATH + "/alerts/" + alertId;

  Firebase.setString(fbdo, basePath + "/type", type);
  Firebase.setString(fbdo, basePath + "/sensor", sensor);
  Firebase.setString(fbdo, basePath + "/message", message);
  Firebase.setString(fbdo, basePath + "/value", value);
  Firebase.setString(fbdo, basePath + "/time", "T+" + String(now / 1000) + "s");
  Firebase.setInt(fbdo, basePath + "/createdAt", now);
  Firebase.setBool(fbdo, basePath + "/acknowledged", false);
}

void pushHistorySnapshot(unsigned long now, int healthScore) {
  String pointPath = DEVICE_PATH + "/history/" + String(now);

  if (!isnan(latestTemp))
    Firebase.setFloat(fbdo, pointPath + "/temperature", latestTemp);
  if (!isnan(latestHum))
    Firebase.setFloat(fbdo, pointPath + "/humidity", latestHum);
  Firebase.setFloat(fbdo, pointPath + "/moisturePct", latestMoisturePct);
  Firebase.setFloat(fbdo, pointPath + "/lux", latestLux);
  Firebase.setInt(fbdo, pointPath + "/health", healthScore);
  Firebase.setString(fbdo, pointPath + "/diagnosis", diagnosis);
  Firebase.setBool(fbdo, pointPath + "/pumpActive", latestPumpOn);
  Firebase.setInt(fbdo, pointPath + "/createdAt", now);
}

void syncControlAndSettings() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (Firebase.getBool(fbdo, DEVICE_PATH + "/control/autoWatering")) {
    autoWateringEnabled = fbdo.boolData();
  }

  if (Firebase.getInt(fbdo, DEVICE_PATH + "/control/pumpDurationSeconds")) {
    int seconds = fbdo.intData();
    if (seconds >= 1 && seconds <= 12) {
      configuredWateringDuration = (unsigned long)seconds * 1000UL;
    }
  }

  if (Firebase.getBool(fbdo, DEVICE_PATH + "/control/manualWaterNow")) {
    manualWaterRequested = fbdo.boolData();
  }

  if (Firebase.getFloat(fbdo, DEVICE_PATH + "/settings/thresholds/soilMin")) {
    thresholdSoilMin = fbdo.floatData();
  }
  if (Firebase.getFloat(fbdo, DEVICE_PATH + "/settings/thresholds/soilMax")) {
    thresholdSoilMax = fbdo.floatData();
  }
  if (Firebase.getFloat(fbdo, DEVICE_PATH + "/settings/thresholds/tempMin")) {
    thresholdTempMin = fbdo.floatData();
  }
  if (Firebase.getFloat(fbdo, DEVICE_PATH + "/settings/thresholds/tempMax")) {
    thresholdTempMax = fbdo.floatData();
  }
  if (Firebase.getFloat(fbdo, DEVICE_PATH + "/settings/thresholds/humidityMin")) {
    thresholdHumidityMin = fbdo.floatData();
  }
  if (Firebase.getFloat(fbdo, DEVICE_PATH + "/settings/thresholds/humidityMax")) {
    thresholdHumidityMax = fbdo.floatData();
  }
  if (Firebase.getFloat(fbdo, DEVICE_PATH + "/settings/thresholds/lightMin")) {
    thresholdLightMin = fbdo.floatData();
  }
  if (Firebase.getFloat(fbdo, DEVICE_PATH + "/settings/thresholds/lightMax")) {
    thresholdLightMax = fbdo.floatData();
  }
}

void evaluateAndPushAlerts(unsigned long now) {
  if (latestMoisturePct < thresholdSoilMin &&
      shouldSendAlert(lastSoilAlertAt, now)) {
    pushAlertToFirebase("warning", "Soil Moisture",
                        "Soil moisture is below optimal range",
                        String(latestMoisturePct, 1) +
                            " (Threshold: " + String(thresholdSoilMin, 1) + ")",
                        now);
  }

  if (!isnan(latestTemp) && latestTemp > thresholdTempMax &&
      shouldSendAlert(lastTempAlertAt, now)) {
    pushAlertToFirebase("critical", "Temperature",
                        "Temperature is above safe threshold",
                        String(latestTemp, 1) +
                            " (Threshold: " + String(thresholdTempMax, 1) + ")",
                        now);
  }

  if (!isnan(latestHum) &&
      (latestHum < thresholdHumidityMin || latestHum > thresholdHumidityMax) &&
      shouldSendAlert(lastHumidityAlertAt, now)) {
    pushAlertToFirebase("warning", "Humidity",
                        "Humidity is outside configured range",
                        String(latestHum, 1) + " (Range: " +
                            String(thresholdHumidityMin, 1) + "-" +
                            String(thresholdHumidityMax, 1) + ")",
                        now);
  }

  if (latestLux < thresholdLightMin && shouldSendAlert(lastLightAlertAt, now)) {
    pushAlertToFirebase("warning", "Light",
                        "Light intensity dropped below minimum",
                        String(latestLux, 0) +
                            " (Threshold: " + String(thresholdLightMin, 0) + ")",
                        now);
  }

  if (isRiskDiagnosis(diagnosis) && shouldSendAlert(lastDiagnosisAlertAt, now)) {
    pushAlertToFirebase("critical", "Diagnosis",
                        "Plant risk detected by diagnosis engine", diagnosis, now);
  }
}

// -------------------- BUZZERS --------------------

void beepRiskAlert() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(RISK_BUZZER_PIN, HIGH);
    delay(180);
    digitalWrite(RISK_BUZZER_PIN, LOW);
    delay(120);
  }
}

void beepWateringAlert() {
  digitalWrite(WATER_BUZZER_PIN, HIGH);
  delay(600);
  digitalWrite(WATER_BUZZER_PIN, LOW);
}

void handleBuzzerAlerts(String currentDiagnosis, bool pumpOn) {
  bool newRisk =
      isRiskDiagnosis(currentDiagnosis) && !isRiskDiagnosis(lastDiagnosis);
  bool wateringStarted = pumpOn && !lastPumpOn;

  if (newRisk) {
    beepRiskAlert();
  }

  if (wateringStarted) {
    beepWateringAlert();
  }

  lastDiagnosis = currentDiagnosis;
  lastPumpOn = pumpOn;
}

// -------------------- SERIAL --------------------

void printLine() {
  Serial.println(
      "============================================================");
}

void printSection() {
  Serial.println(
      "------------------------------------------------------------");
}

void printSensorData() {
  printLine();
  Serial.println("              LIVE SENSOR DATASET OUTPUT");
  printLine();

  Serial.print("temperature   : ");
  if (isnan(latestTemp))
    Serial.println("null");
  else
    Serial.println(latestTemp, 2);

  Serial.print("humidity_pct  : ");
  if (isnan(latestHum))
    Serial.println("null");
  else
    Serial.println(latestHum, 2);

  Serial.print("raw_soil      : ");
  Serial.println(latestRawSoil);

  Serial.print("moisture_pct  : ");
  Serial.println(latestMoisturePct, 2);

  Serial.print("lux           : ");
  Serial.println(latestLux, 2);

  printSection();

  Serial.print("diagnosis     : ");
  Serial.println(diagnosis);

  Serial.print("pump_status   : ");
  Serial.println(latestPumpOn ? "ON" : "OFF");

  Serial.print("recommendation: ");
  Serial.println(recommendation);

  Serial.print("cooldown      : ");
  Serial.println(cooldownActive ? "ACTIVE" : "READY");

  printLine();
  Serial.println();
}

// -------------------- OLED --------------------

void drawLeafIconBig(int x, int y) {
  display.drawRoundRect(x, y, 18, 12, 6, WHITE);
  display.drawLine(x + 9, y + 6, x + 9, y + 16, WHITE);
  display.drawLine(x + 9, y + 12, x + 14, y + 7, WHITE);
}

void drawCenteredText(String text, int y, int size) {
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextSize(size);
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  display.setCursor(x, y);
  display.print(text);
}

void drawFrame() { display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, WHITE); }

void showStartupScreen() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  drawFrame();

  drawLeafIconBig(55, 4);

  drawCenteredText("AGRI_BLOOM", 22, 2);
  drawCenteredText("SMART PLANT", 46, 1);
  drawCenteredText("MONITOR", 56, 1);

  display.display();
}

void showTempHumidityScreen(float temp, float hum) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  drawFrame();

  drawCenteredText("TEMPERATURE", 4, 1);
  if (isnan(temp))
    drawCenteredText("NULL", 18, 2);
  else
    drawCenteredText(String((int)temp) + " C", 18, 2);

  drawCenteredText("HUMIDITY", 42, 1);
  if (isnan(hum))
    drawCenteredText("NULL", 54, 1);
  else
    drawCenteredText(String((int)hum) + " %", 54, 1);

  display.display();
}

void showMoistureLuxScreen(float moisturePct, float lux) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  drawFrame();

  drawCenteredText("MOISTURE", 4, 1);
  drawCenteredText(String((int)moisturePct) + " %", 18, 2);

  drawCenteredText("LUX", 42, 1);
  drawCenteredText(String((int)lux), 54, 1);

  display.display();
}

void showDiagnosisScreen(String diagnosis) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  drawFrame();

  drawCenteredText("DIAGNOSIS", 4, 1);

  if (diagnosis == "Healthy") {
    drawCenteredText("HEALTHY", 24, 2);
  } else if (diagnosis == "Water Stress") {
    drawCenteredText("WATER", 20, 2);
    drawCenteredText("STRESS", 42, 2);
  } else if (diagnosis == "Heat Stress") {
    drawCenteredText("HEAT", 20, 2);
    drawCenteredText("STRESS", 42, 2);
  } else if (diagnosis == "Fungal Risk") {
    drawCenteredText("FUNGAL", 20, 2);
    drawCenteredText("RISK", 42, 2);
  } else if (diagnosis == "Root Rot Risk") {
    drawCenteredText("ROOT ROT", 20, 2);
    drawCenteredText("RISK", 42, 2);
  } else if (diagnosis == "Light Deficiency") {
    drawCenteredText("LIGHT", 20, 2);
    drawCenteredText("DEFICIENCY", 42, 1);
  } else if (diagnosis == "Nutrient Deficiency") {
    drawCenteredText("NUTRIENT", 20, 1);
    drawCenteredText("DEFICIENCY", 38, 1);
  } else {
    drawCenteredText("UNKNOWN", 28, 2);
  }

  display.display();
}

void showRecommendationScreen(String recommendation) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  drawFrame();

  drawCenteredText("RECOMMEND", 4, 1);

  if (recommendation == "Water the plant now") {
    drawCenteredText("WATER", 22, 2);
    drawCenteredText("NOW", 46, 2);
  } else if (recommendation == "Watering now") {
    drawCenteredText("WATERING", 22, 1);
    drawCenteredText("NOW", 42, 2);
  } else if (recommendation == "Waiting to recheck") {
    drawCenteredText("WAITING", 22, 1);
    drawCenteredText("RECHECK", 42, 1);
  } else if (recommendation == "Stop watering") {
    drawCenteredText("STOP", 22, 2);
    drawCenteredText("WATERING", 46, 1);
  } else if (recommendation == "Cool the plant") {
    drawCenteredText("COOL THE", 22, 2);
    drawCenteredText("PLANT", 46, 2);
  } else if (recommendation == "Reduce moisture") {
    drawCenteredText("REDUCE", 22, 2);
    drawCenteredText("MOISTURE", 46, 1);
  } else if (recommendation == "Increase light") {
    drawCenteredText("INCREASE", 22, 1);
    drawCenteredText("LIGHT", 42, 2);
  } else if (recommendation == "Check fertilizer") {
    drawCenteredText("CHECK", 22, 2);
    drawCenteredText("FERTILIZER", 46, 1);
  } else if (recommendation == "Plant is stable") {
    drawCenteredText("PLANT IS", 22, 2);
    drawCenteredText("STABLE", 46, 2);
  } else {
    drawCenteredText("MONITOR", 22, 2);
    drawCenteredText("CONDITIONS", 46, 1);
  }

  display.display();
}

void updateOLED(float temp, float hum, float moisturePct, float lux,
                String diagnosis, String recommendation) {
  unsigned long now = millis();

  if (!startupDone) {
    showStartupScreen();
    if (now - screenTimer >= STARTUP_DURATION) {
      startupDone = true;
      currentScreen = 2;
      screenTimer = now;
    }
    return;
  }

  if (now - screenTimer >= SCREEN_DURATION) {
    currentScreen++;
    if (currentScreen > 5)
      currentScreen = 2;
    screenTimer = now;
  }

  switch (currentScreen) {
  case 2:
    showTempHumidityScreen(temp, hum);
    break;
  case 3:
    showMoistureLuxScreen(moisturePct, lux);
    break;
  case 4:
    showDiagnosisScreen(diagnosis);
    break;
  case 5:
    showRecommendationScreen(recommendation);
    break;
  }
}

// -------------------- PUMP CONTROL --------------------

void startWatering(unsigned long now) {
  wateringActive = true;
  cooldownActive = false;
  wateringStartTime = now;
  latestPumpOn = true;
  digitalWrite(PUMP_PIN, HIGH); // change to LOW if your module is inverted
}

void stopWateringAndStartCooldown(unsigned long now) {
  wateringActive = false;
  cooldownActive = true;
  cooldownStartTime = now;
  latestPumpOn = false;
  digitalWrite(PUMP_PIN, LOW); // change to HIGH if your module is inverted
}

void forcePumpOff() {
  latestPumpOn = false;
  digitalWrite(PUMP_PIN, LOW); // change to HIGH if your module is inverted
}

// -------------------- SENSOR + LOGIC UPDATE --------------------

void updateSensorsAndLogic() {
  unsigned long now = millis();

  latestTemp = dht.readTemperature();
  latestHum = dht.readHumidity();

  latestRawSoil = readAveragedAnalog(SOIL_PIN, SOIL_SAMPLES);
  float freshMoisture = getMoisturePct(latestRawSoil);
  latestMoisturePct = smoothMoisture(freshMoisture);

  latestLux = lightMeter.readLightLevel();
  if (latestLux < 0 || latestLux > 100000)
    latestLux = 0;

  if (!isnan(latestTemp) && !isnan(latestHum)) {
    diagnosis =
        diagnosePlant(latestMoisturePct, latestLux, latestTemp, latestHum);
  } else {
    diagnosis = "Unknown";
  }

  syncControlAndSettings();

  if (cooldownActive && (now - cooldownStartTime >= WATERING_COOLDOWN)) {
    cooldownActive = false;
  }

  if (manualWaterRequested && !wateringActive) {
    cooldownActive = false;
    manualWaterRequested = false;
    Firebase.setBool(fbdo, DEVICE_PATH + "/control/manualWaterNow", false);
    startWatering(now);
    pushAlertToFirebase("info", "Pump", "Manual watering command executed",
                        "Run time: " +
                            String((int)(configuredWateringDuration / 1000)) + "s",
                        now);
  }

  if (!wateringActive && !cooldownActive) {
    if (autoWateringEnabled && diagnosis == "Water Stress") {
      startWatering(now);
    } else {
      forcePumpOff();
    }
  }

  recommendation = getRecommendation(diagnosis, wateringActive, cooldownActive);
  int healthScore = computeHealthScore(diagnosis, latestMoisturePct, latestTemp,
                                       latestHum, latestLux);
  handleBuzzerAlerts(diagnosis, latestPumpOn);
  printSensorData();

  if (WiFi.status() == WL_CONNECTED) {
    evaluateAndPushAlerts(now);

    if (!isnan(latestTemp))
      Firebase.setFloat(fbdo, DEVICE_PATH + "/sensors/temperature", latestTemp);
    if (!isnan(latestHum))
      Firebase.setFloat(fbdo, DEVICE_PATH + "/sensors/humidity", latestHum);
    Firebase.setFloat(fbdo, DEVICE_PATH + "/sensors/moisturePct",
                      latestMoisturePct);
    Firebase.setFloat(fbdo, DEVICE_PATH + "/sensors/lux", latestLux);
    Firebase.setString(fbdo, DEVICE_PATH + "/status/diagnosis", diagnosis);
    Firebase.setString(fbdo, DEVICE_PATH + "/status/recommendation",
                       recommendation);
    Firebase.setBool(fbdo, DEVICE_PATH + "/status/pumpActive", latestPumpOn);
    Firebase.setInt(fbdo, DEVICE_PATH + "/status/healthScore", healthScore);
    Firebase.setBool(fbdo, DEVICE_PATH + "/control/autoWatering",
                     autoWateringEnabled);
    Firebase.setInt(fbdo, DEVICE_PATH + "/control/pumpDurationSeconds",
                    configuredWateringDuration / 1000);

    pushHistorySnapshot(now, healthScore);
  }
}

// -------------------- SETUP --------------------

void setup() {
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  int wifi_attempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_attempts < 20) {
    Serial.print(".");
    delay(500);
    wifi_attempts++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to Wi-Fi!");
  } else {
    Serial.println("Wi-Fi connection failed.");
  }

  config.host = FIREBASE_HOST;
  config.api_key = FIREBASE_AUTH;
  config.signer.tokens.legacy_token =
      FIREBASE_AUTH; // Provide API key as both to ensure compatibility
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (!Firebase.getBool(fbdo, DEVICE_PATH + "/control/autoWatering")) {
    Firebase.setBool(fbdo, DEVICE_PATH + "/control/autoWatering", true);
  }
  if (!Firebase.getInt(fbdo, DEVICE_PATH + "/control/pumpDurationSeconds")) {
    Firebase.setInt(fbdo, DEVICE_PATH + "/control/pumpDurationSeconds",
                    WATERING_DURATION / 1000);
  }
  Firebase.setBool(fbdo, DEVICE_PATH + "/control/manualWaterNow", false);
  if (!Firebase.getBool(fbdo, DEVICE_PATH + "/settings/notificationsEnabled")) {
    Firebase.setBool(fbdo, DEVICE_PATH + "/settings/notificationsEnabled",
                     true);
  }
  if (!Firebase.getFloat(fbdo, DEVICE_PATH + "/settings/thresholds/soilMin")) {
    Firebase.setFloat(fbdo, DEVICE_PATH + "/settings/thresholds/soilMin",
                      60.0);
  }
  if (!Firebase.getFloat(fbdo, DEVICE_PATH + "/settings/thresholds/soilMax")) {
    Firebase.setFloat(fbdo, DEVICE_PATH + "/settings/thresholds/soilMax",
                      70.0);
  }
  if (!Firebase.getFloat(fbdo, DEVICE_PATH + "/settings/thresholds/tempMin")) {
    Firebase.setFloat(fbdo, DEVICE_PATH + "/settings/thresholds/tempMin",
                      18.0);
  }
  if (!Firebase.getFloat(fbdo, DEVICE_PATH + "/settings/thresholds/tempMax")) {
    Firebase.setFloat(fbdo, DEVICE_PATH + "/settings/thresholds/tempMax",
                      27.0);
  }
  if (!Firebase.getFloat(fbdo,
                         DEVICE_PATH + "/settings/thresholds/humidityMin")) {
    Firebase.setFloat(fbdo, DEVICE_PATH + "/settings/thresholds/humidityMin",
                      50.0);
  }
  if (!Firebase.getFloat(fbdo,
                         DEVICE_PATH + "/settings/thresholds/humidityMax")) {
    Firebase.setFloat(fbdo, DEVICE_PATH + "/settings/thresholds/humidityMax",
                      80.0);
  }
  if (!Firebase.getFloat(fbdo, DEVICE_PATH + "/settings/thresholds/lightMin")) {
    Firebase.setFloat(fbdo, DEVICE_PATH + "/settings/thresholds/lightMin",
                      5000.0);
  }
  if (!Firebase.getFloat(fbdo, DEVICE_PATH + "/settings/thresholds/lightMax")) {
    Firebase.setFloat(fbdo, DEVICE_PATH + "/settings/thresholds/lightMax",
                      5500.0);
  }

  dht.begin();

  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW); // change to HIGH if your module is inverted

  pinMode(RISK_BUZZER_PIN, OUTPUT);
  pinMode(WATER_BUZZER_PIN, OUTPUT);
  digitalWrite(RISK_BUZZER_PIN, LOW);
  digitalWrite(WATER_BUZZER_PIN, LOW);

  analogReadResolution(12);

  Wire.begin(I2C_SDA, I2C_SCL);

  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("BH1750 STARTED SUCCESSFULLY.");
  } else {
    Serial.println("BH1750 FAILED TO START.");
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED NOT FOUND");
    while (true)
      ;
  }

  screenTimer = millis();
  sensorTimer = millis();

  showStartupScreen();

  Serial.println();
  printLine();
  Serial.println("              SMART PLANT MONITORING SYSTEM");
  printLine();
  Serial.println(" OLED + BH1750 + DHT22 + SOIL + PUMP + 2 BUZZERS");
  printLine();

  updateSensorsAndLogic();
}

// -------------------- LOOP --------------------

void loop() {
  unsigned long now = millis();

  updateOLED(latestTemp, latestHum, latestMoisturePct, latestLux, diagnosis,
             recommendation);

  if (wateringActive && (now - wateringStartTime >= configuredWateringDuration)) {
    stopWateringAndStartCooldown(now);
    recommendation =
        getRecommendation(diagnosis, wateringActive, cooldownActive);
  }

  if (now - sensorTimer >= SENSOR_INTERVAL) {
    sensorTimer = now;
    updateSensorsAndLogic();
  }

  delay(50);
}
