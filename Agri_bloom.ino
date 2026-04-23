
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BH1750.h>
#include <DHT.h>
#include <WiFi.h>
#include <Wire.h>
#include <math.h>

#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

#include "model.h"
#include "scaler_params.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/tflite_bridge/micro_error_reporter.h"
#include "tensorflow/lite/schema/schema_generated.h"

// -------------------- WIFI + FIREBASE --------------------
#define WIFI_SSID "Dialog 4G 635"
#define WIFI_PASSWORD "1c915bDF"

#define FIREBASE_API_KEY "AIzaSyDo20_bhC1QcwWB86yiZA2mjvo9uDw1aC8"
#define FIREBASE_DATABASE_URL "https://agri-bloom-59b2a-default-rtdb.firebaseio.com/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

// -------------------- PINS --------------------
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
const unsigned long STARTUP_DURATION   = 5000;
const unsigned long SCREEN_DURATION    = 5000;
const unsigned long SENSOR_INTERVAL    = 30000;
const unsigned long CONTROL_INTERVAL   = 1000;
const unsigned long WATERING_DURATION  = 5000;
const unsigned long WATERING_COOLDOWN  = 55000;

// -------------------- OBJECTS --------------------
DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// -------------------- STATE --------------------
String diagnosis = "Unknown";
String recommendation = "Monitor plant conditions";

String lastDiagnosis = "";
bool lastPumpOn = false;

unsigned long screenTimer = 0;
unsigned long sensorTimer = 0;
unsigned long controlTimer = 0;
unsigned long wateringStartTime = 0;
unsigned long cooldownStartTime = 0;

int currentScreen = 1;
bool startupDone = false;

bool wateringActive = false;
bool cooldownActive = false;
int waterStressCount = 0;

// latest sensor values
float latestTemp = NAN;
float latestHum = NAN;
float latestMoisturePct = 0.0;
float latestLux = 0.0;
int latestRawSoil = 0;
bool latestPumpOn = false;

const String DEVICE_PATH = "/AgriBloom/device_001";

unsigned long manualWateringDuration = WATERING_DURATION;
unsigned long activeWateringDuration = WATERING_DURATION;
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

// -------------------- TFLITE GLOBALS --------------------
namespace {
  tflite::MicroErrorReporter micro_error_reporter;
  tflite::ErrorReporter* error_reporter = &micro_error_reporter;
  const tflite::Model* model = nullptr;
  tflite::MicroInterpreter* interpreter = nullptr;
  TfLiteTensor* input_tensor = nullptr;
  TfLiteTensor* output_tensor = nullptr;

  constexpr int kTensorArenaSize = 8 * 1024;
  uint8_t tensor_arena[kTensorArenaSize];
}

// -------------------- WELFORD GLOBALS --------------------
uint32_t welf_count = 0;

float welf_mean[4] = {
  SENSOR_MEANS[0],
  SENSOR_MEANS[1],
  SENSOR_MEANS[2],
  SENSOR_MEANS[3]
};

float welf_m2[4] = {0, 0, 0, 0};

float welf_variance[4] = {
  SENSOR_STDS[0] * SENSOR_STDS[0],
  SENSOR_STDS[1] * SENSOR_STDS[1],
  SENSOR_STDS[2] * SENSOR_STDS[2],
  SENSOR_STDS[3] * SENSOR_STDS[3]
};

#define WELFORD_SAVE_EVERY 50

// -------------------- FIREBASE HELPERS --------------------
bool firebaseReadyForRTDB() {
  return WiFi.status() == WL_CONNECTED && Firebase.ready() && signupOK;
}

bool dbSetString(const String &path, const String &value) {
  if (!firebaseReadyForRTDB()) return false;
  return Firebase.RTDB.setString(&fbdo, path.c_str(), value);
}

bool dbSetFloat(const String &path, float value) {
  if (!firebaseReadyForRTDB()) return false;
  return Firebase.RTDB.setFloat(&fbdo, path.c_str(), value);
}

bool dbSetInt(const String &path, int value) {
  if (!firebaseReadyForRTDB()) return false;
  return Firebase.RTDB.setInt(&fbdo, path.c_str(), value);
}

bool dbSetBool(const String &path, bool value) {
  if (!firebaseReadyForRTDB()) return false;
  return Firebase.RTDB.setBool(&fbdo, path.c_str(), value);
}

bool dbGetBool(const String &path, bool &outValue) {
  if (!firebaseReadyForRTDB()) return false;
  if (Firebase.RTDB.getBool(&fbdo, path.c_str())) {
    outValue = fbdo.boolData();
    return true;
  }
  return false;
}

bool dbGetInt(const String &path, int &outValue) {
  if (!firebaseReadyForRTDB()) return false;
  if (Firebase.RTDB.getInt(&fbdo, path.c_str())) {
    outValue = fbdo.intData();
    return true;
  }
  return false;
}

bool dbGetFloat(const String &path, float &outValue) {
  if (!firebaseReadyForRTDB()) return false;
  if (Firebase.RTDB.getFloat(&fbdo, path.c_str())) {
    outValue = fbdo.floatData();
    return true;
  }
  return false;
}

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
  float moisturePct = ((float)(SOIL_DRY_VALUE - rawSoil) /
                       (SOIL_DRY_VALUE - SOIL_WET_VALUE)) * 100.0;

  if (moisturePct < 0) moisturePct = 0;
  if (moisturePct > 100) moisturePct = 100;

  return moisturePct;
}

float smoothMoisture(float currentValue) {
  static float lastValue = currentValue;
  float smoothed = (lastValue * 0.7f) + (currentValue * 0.3f);
  lastValue = smoothed;
  return smoothed;
}

// -------------------- WELFORD FUNCTIONS --------------------
void welfordUpdate(float moisture, float lux, float temp, float hum) {
  float values[4] = { moisture, lux, temp, hum };
  welf_count++;

  for (int ch = 0; ch < 4; ch++) {
    float delta = values[ch] - welf_mean[ch];
    welf_mean[ch] += delta / welf_count;
    float delta2 = values[ch] - welf_mean[ch];
    welf_m2[ch] += delta * delta2;

    if (welf_count > 1) {
      welf_variance[ch] = welf_m2[ch] / welf_count;
    }
  }

  if (welf_count % WELFORD_SAVE_EVERY == 0) {
    Serial.println("[Welford] Learned parameters updated.");
  }
}

// Prediction uses fixed scaler from training
float welfordNormalise(float value, int channel) {
  return (value - SENSOR_MEANS[channel]) / SENSOR_STDS[channel];
}

// Let Welford learn only from safer states
bool shouldUpdateWelford(String diag) {
  return (
    diag == "Healthy" ||
    diag == "Light Deficiency" ||
    diag == "Nutrient Deficiency"
  );
}

// -------------------- ML DIAGNOSIS --------------------
String diagnosePlant(float moisturePct, float lux, float temp, float hum) {
  if (interpreter == nullptr || input_tensor == nullptr || output_tensor == nullptr) {
    return "Unknown";
  }

  float n_moisture = welfordNormalise(moisturePct, 0);
  float n_lux      = welfordNormalise(lux, 1);
  float n_temp     = welfordNormalise(temp, 2);
  float n_hum      = welfordNormalise(hum, 3);

  float input_scale = input_tensor->params.scale;
  int input_zero_point = input_tensor->params.zero_point;

  input_tensor->data.int8[0] = (int8_t)(n_moisture / input_scale + input_zero_point);
  input_tensor->data.int8[1] = (int8_t)(n_lux      / input_scale + input_zero_point);
  input_tensor->data.int8[2] = (int8_t)(n_temp     / input_scale + input_zero_point);
  input_tensor->data.int8[3] = (int8_t)(n_hum      / input_scale + input_zero_point);

  TfLiteStatus status = interpreter->Invoke();
  if (status != kTfLiteOk) {
    Serial.println("[ML] Inference failed!");
    return "Unknown";
  }

  float output_scale = output_tensor->params.scale;
  int output_zero_point = output_tensor->params.zero_point;

  float probs[7];
  for (int i = 0; i < 7; i++) {
    probs[i] = (output_tensor->data.int8[i] - output_zero_point) * output_scale;
  }

  int best_idx = 0;
  float best_prob = probs[0];

  for (int i = 1; i < 7; i++) {
    if (probs[i] > best_prob) {
      best_prob = probs[i];
      best_idx = i;
    }
  }

  if (best_prob < 0.60f) {
    Serial.printf("[ML] Low confidence: %.1f%% -> Unknown\n", best_prob * 100.0f);
    return "Unknown";
  }

  const char* labels[7] = {
    "Fungal Risk",
    "Healthy",
    "Heat Stress",
    "Light Deficiency",
    "Nutrient Deficiency",
    "Root Rot Risk",
    "Water Stress"
  };

  Serial.printf("[ML] Diagnosis: %s (%.1f%%)\n", labels[best_idx], best_prob * 100.0f);
  return String(labels[best_idx]);
}

bool isRiskDiagnosis(String diagnosis) {
  return (
    diagnosis == "Water Stress" ||
    diagnosis == "Root Rot Risk" ||
    diagnosis == "Heat Stress" ||
    diagnosis == "Fungal Risk"
  );
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

  if (currentDiagnosis == "Water Stress") score -= 28;
  else if (currentDiagnosis == "Root Rot Risk") score -= 30;
  else if (currentDiagnosis == "Heat Stress") score -= 24;
  else if (currentDiagnosis == "Fungal Risk") score -= 26;
  else if (currentDiagnosis == "Light Deficiency") score -= 16;
  else if (currentDiagnosis == "Nutrient Deficiency") score -= 14;
  else if (currentDiagnosis == "Unknown") score -= 20;

  if (moisturePct < thresholdSoilMin) score -= 6;
  if (moisturePct > thresholdSoilMax) score -= 6;
  if (temp < thresholdTempMin) score -= 5;
  if (temp > thresholdTempMax) score -= 8;
  if (hum < thresholdHumidityMin || hum > thresholdHumidityMax) score -= 5;
  if (lux < thresholdLightMin || lux > thresholdLightMax) score -= 5;

  if (score < 0) score = 0;
  if (score > 100) score = 100;

  return score;
}

bool shouldSendAlert(unsigned long &lastSentAt, unsigned long now) {
  if (now - lastSentAt < ALERT_THROTTLE) return false;
  lastSentAt = now;
  return true;
}

void pushAlertToFirebase(String type, String sensor, String message, String value,
                         unsigned long now) {
  String alertId = String(now) + "_" + String(random(100, 999));
  String basePath = DEVICE_PATH + "/alerts/" + alertId;

  dbSetString(basePath + "/type", type);
  dbSetString(basePath + "/sensor", sensor);
  dbSetString(basePath + "/message", message);
  dbSetString(basePath + "/value", value);
  dbSetString(basePath + "/time", "T+" + String(now / 1000) + "s");
  dbSetInt(basePath + "/createdAt", (int)now);
  dbSetBool(basePath + "/acknowledged", false);
}

void pushHistorySnapshot(unsigned long now, int healthScore) {
  String pointPath = DEVICE_PATH + "/history/" + String(now);

  if (!isnan(latestTemp)) dbSetFloat(pointPath + "/temperature", latestTemp);
  if (!isnan(latestHum)) dbSetFloat(pointPath + "/humidity", latestHum);
  dbSetFloat(pointPath + "/moisturePct", latestMoisturePct);
  dbSetFloat(pointPath + "/lux", latestLux);
  dbSetInt(pointPath + "/health", healthScore);
  dbSetString(pointPath + "/diagnosis", diagnosis);
  dbSetBool(pointPath + "/pumpActive", latestPumpOn);
  dbSetInt(pointPath + "/createdAt", (int)now);
}

void syncControlAndSettings() {
  if (!firebaseReadyForRTDB()) return;

  bool boolValue;
  int intValue;
  float floatValue;

  if (dbGetBool(DEVICE_PATH + "/control/autoWatering", boolValue)) {
    autoWateringEnabled = boolValue;
  }

  if (dbGetInt(DEVICE_PATH + "/control/pumpDurationSeconds", intValue)) {
    if (intValue >= 1 && intValue <= 120) {
      manualWateringDuration = (unsigned long)intValue * 1000UL;
    }
  }

  if (dbGetBool(DEVICE_PATH + "/control/manualWaterNow", boolValue)) {
    manualWaterRequested = boolValue;
  }

  if (dbGetFloat(DEVICE_PATH + "/settings/thresholds/soilMin", floatValue)) {
    thresholdSoilMin = floatValue;
  }
  if (dbGetFloat(DEVICE_PATH + "/settings/thresholds/soilMax", floatValue)) {
    thresholdSoilMax = floatValue;
  }
  if (dbGetFloat(DEVICE_PATH + "/settings/thresholds/tempMin", floatValue)) {
    thresholdTempMin = floatValue;
  }
  if (dbGetFloat(DEVICE_PATH + "/settings/thresholds/tempMax", floatValue)) {
    thresholdTempMax = floatValue;
  }
  if (dbGetFloat(DEVICE_PATH + "/settings/thresholds/humidityMin", floatValue)) {
    thresholdHumidityMin = floatValue;
  }
  if (dbGetFloat(DEVICE_PATH + "/settings/thresholds/humidityMax", floatValue)) {
    thresholdHumidityMax = floatValue;
  }
  if (dbGetFloat(DEVICE_PATH + "/settings/thresholds/lightMin", floatValue)) {
    thresholdLightMin = floatValue;
  }
  if (dbGetFloat(DEVICE_PATH + "/settings/thresholds/lightMax", floatValue)) {
    thresholdLightMax = floatValue;
  }
}

void evaluateAndPushAlerts(unsigned long now) {
  if (latestMoisturePct < thresholdSoilMin &&
      shouldSendAlert(lastSoilAlertAt, now)) {
    pushAlertToFirebase(
        "warning", "Soil Moisture",
        "Soil moisture is below optimal range",
        String(latestMoisturePct, 1) + " (Threshold: " + String(thresholdSoilMin, 1) + ")",
        now);
  }

  if (!isnan(latestTemp) && latestTemp > thresholdTempMax &&
      shouldSendAlert(lastTempAlertAt, now)) {
    pushAlertToFirebase(
        "critical", "Temperature",
        "Temperature is above safe threshold",
        String(latestTemp, 1) + " (Threshold: " + String(thresholdTempMax, 1) + ")",
        now);
  }

  if (!isnan(latestHum) &&
      (latestHum < thresholdHumidityMin || latestHum > thresholdHumidityMax) &&
      shouldSendAlert(lastHumidityAlertAt, now)) {
    pushAlertToFirebase(
        "warning", "Humidity",
        "Humidity is outside configured range",
        String(latestHum, 1) + " (Range: " + String(thresholdHumidityMin, 1) + "-" +
            String(thresholdHumidityMax, 1) + ")",
        now);
  }

  if (latestLux < thresholdLightMin &&
      shouldSendAlert(lastLightAlertAt, now)) {
    pushAlertToFirebase(
        "warning", "Light",
        "Light intensity dropped below minimum",
        String(latestLux, 0) + " (Threshold: " + String(thresholdLightMin, 0) + ")",
        now);
  }

  if (isRiskDiagnosis(diagnosis) &&
      shouldSendAlert(lastDiagnosisAlertAt, now)) {
    pushAlertToFirebase("critical", "Diagnosis",
                        "Plant risk detected by diagnosis engine",
                        diagnosis, now);
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
  bool newRisk = isRiskDiagnosis(currentDiagnosis) && !isRiskDiagnosis(lastDiagnosis);
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
  Serial.println("============================================================");
}

void printSection() {
  Serial.println("------------------------------------------------------------");
}

void printSensorData() {
  printLine();
  Serial.println("              LIVE SENSOR DATASET OUTPUT");
  printLine();

  Serial.print("temperature   : ");
  if (isnan(latestTemp)) Serial.println("null");
  else Serial.println(latestTemp, 2);

  Serial.print("humidity_pct  : ");
  if (isnan(latestHum)) Serial.println("null");
  else Serial.println(latestHum, 2);

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

  Serial.print("water_stress_count : ");
  Serial.println(waterStressCount);

  Serial.print("welford_count : ");
  Serial.println(welf_count);

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

void drawFrame() {
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);
}

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
  if (isnan(temp)) drawCenteredText("NULL", 18, 2);
  else drawCenteredText(String((int)temp) + " C", 18, 2);

  drawCenteredText("HUMIDITY", 42, 1);
  if (isnan(hum)) drawCenteredText("NULL", 54, 1);
  else drawCenteredText(String((int)hum) + " %", 54, 1);

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
    if (currentScreen > 5) currentScreen = 2;
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
  digitalWrite(PUMP_PIN, HIGH);
}

void stopWateringAndStartCooldown(unsigned long now) {
  wateringActive = false;
  cooldownActive = true;
  cooldownStartTime = now;
  latestPumpOn = false;
  digitalWrite(PUMP_PIN, LOW);
}

void forcePumpOff() {
  latestPumpOn = false;
  digitalWrite(PUMP_PIN, LOW);
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
  if (latestLux < 0 || latestLux > 100000) latestLux = 0;

  if (!isnan(latestTemp) && !isnan(latestHum)) {
    diagnosis = diagnosePlant(latestMoisturePct, latestLux, latestTemp, latestHum);

    if (shouldUpdateWelford(diagnosis)) {
      welfordUpdate(latestMoisturePct, latestLux, latestTemp, latestHum);
    }
  } else {
    diagnosis = "Unknown";
  }

  syncControlAndSettings();

  if (diagnosis == "Water Stress") {
    waterStressCount++;
  } else {
    waterStressCount = 0;
  }

  if (cooldownActive && (now - cooldownStartTime >= WATERING_COOLDOWN)) {
    cooldownActive = false;
  }

  if (manualWaterRequested && !wateringActive) {
    cooldownActive = false;
    manualWaterRequested = false;
    dbSetBool(DEVICE_PATH + "/control/manualWaterNow", false);
    activeWateringDuration = manualWateringDuration;
    startWatering(now);
    pushAlertToFirebase("info", "Pump", "Manual watering command executed",
                        "Run time: " + String((int)(manualWateringDuration / 1000)) + "s",
                        now);
    waterStressCount = 0;
  }

  if (!wateringActive && !cooldownActive) {
    if (autoWateringEnabled) {
      if (manualWaterRequested) {
        // handled above
      } else if (waterStressCount >= 2) {
        activeWateringDuration = WATERING_DURATION;
        startWatering(now);
        waterStressCount = 0;
      } else {
        forcePumpOff();
      }
    } else {
      forcePumpOff();
    }
  }

  recommendation = getRecommendation(diagnosis, wateringActive, cooldownActive);
  int healthScore = computeHealthScore(diagnosis, latestMoisturePct, latestTemp,
                                       latestHum, latestLux);

  handleBuzzerAlerts(diagnosis, latestPumpOn);
  printSensorData();

  if (firebaseReadyForRTDB()) {
    evaluateAndPushAlerts(now);

    if (!isnan(latestTemp)) dbSetFloat(DEVICE_PATH + "/sensors/temperature", latestTemp);
    if (!isnan(latestHum)) dbSetFloat(DEVICE_PATH + "/sensors/humidity", latestHum);
    dbSetFloat(DEVICE_PATH + "/sensors/moisturePct", latestMoisturePct);
    dbSetFloat(DEVICE_PATH + "/sensors/lux", latestLux);

    dbSetString(DEVICE_PATH + "/status/diagnosis", diagnosis);
    dbSetString(DEVICE_PATH + "/status/recommendation", recommendation);
    dbSetBool(DEVICE_PATH + "/status/pumpActive", latestPumpOn);
    dbSetInt(DEVICE_PATH + "/status/healthScore", healthScore);

    dbSetBool(DEVICE_PATH + "/control/autoWatering", autoWateringEnabled);
    dbSetInt(DEVICE_PATH + "/control/pumpDurationSeconds",
             manualWateringDuration / 1000);

    pushHistorySnapshot(now, healthScore);
  }
}

// -------------------- SETUP --------------------
void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);

  pinMode(RISK_BUZZER_PIN, OUTPUT);
  pinMode(WATER_BUZZER_PIN, OUTPUT);
  digitalWrite(RISK_BUZZER_PIN, LOW);
  digitalWrite(WATER_BUZZER_PIN, LOW);

  analogReadResolution(12);

  Wire.begin(I2C_SDA, I2C_SCL);

  model = tflite::GetModel(agri_bloom_model_data);

  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("[ML] Model schema version mismatch!");
    while (true);
  }

  static tflite::MicroMutableOpResolver<5> resolver;
  resolver.AddFullyConnected();
  resolver.AddSoftmax();
  resolver.AddRelu();
  resolver.AddQuantize();
  resolver.AddDequantize();

  static tflite::MicroInterpreter static_interpreter(
    model, resolver, tensor_arena, kTensorArenaSize
  );

  interpreter = &static_interpreter;

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("[ML] AllocateTensors failed!");
    while (true);
  }

  input_tensor = interpreter->input(0);
  output_tensor = interpreter->output(0);

  Serial.printf("[ML] TFLite ready. Input: %d  Output: %d\n",
                input_tensor->dims->data[1],
                output_tensor->dims->data[1]);

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

  config.api_key = FIREBASE_API_KEY;
  config.database_url = FIREBASE_DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    signupOK = true;
    Serial.println("Firebase signUp OK");
  } else {
    signupOK = false;
    Serial.print("Firebase signUp failed: ");
    Serial.println(config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (signupOK) {
    bool b;
    float f;
    int i;

    if (!dbGetBool(DEVICE_PATH + "/control/autoWatering", b)) {
      dbSetBool(DEVICE_PATH + "/control/autoWatering", true);
    }
    if (!dbGetInt(DEVICE_PATH + "/control/pumpDurationSeconds", i)) {
      dbSetInt(DEVICE_PATH + "/control/pumpDurationSeconds", WATERING_DURATION / 1000);
    } else if (i >= 1 && i <= 60) {
      manualWateringDuration = (unsigned long)i * 1000UL;
    }

    dbSetBool(DEVICE_PATH + "/control/manualWaterNow", false);

    if (!dbGetBool(DEVICE_PATH + "/settings/notificationsEnabled", b)) {
      dbSetBool(DEVICE_PATH + "/settings/notificationsEnabled", true);
    }
    if (!dbGetFloat(DEVICE_PATH + "/settings/thresholds/soilMin", f)) {
      dbSetFloat(DEVICE_PATH + "/settings/thresholds/soilMin", 60.0);
    }
    if (!dbGetFloat(DEVICE_PATH + "/settings/thresholds/soilMax", f)) {
      dbSetFloat(DEVICE_PATH + "/settings/thresholds/soilMax", 70.0);
    }
    if (!dbGetFloat(DEVICE_PATH + "/settings/thresholds/tempMin", f)) {
      dbSetFloat(DEVICE_PATH + "/settings/thresholds/tempMin", 18.0);
    }
    if (!dbGetFloat(DEVICE_PATH + "/settings/thresholds/tempMax", f)) {
      dbSetFloat(DEVICE_PATH + "/settings/thresholds/tempMax", 27.0);
    }
    if (!dbGetFloat(DEVICE_PATH + "/settings/thresholds/humidityMin", f)) {
      dbSetFloat(DEVICE_PATH + "/settings/thresholds/humidityMin", 50.0);
    }
    if (!dbGetFloat(DEVICE_PATH + "/settings/thresholds/humidityMax", f)) {
      dbSetFloat(DEVICE_PATH + "/settings/thresholds/humidityMax", 80.0);
    }
    if (!dbGetFloat(DEVICE_PATH + "/settings/thresholds/lightMin", f)) {
      dbSetFloat(DEVICE_PATH + "/settings/thresholds/lightMin", 5000.0);
    }
    if (!dbGetFloat(DEVICE_PATH + "/settings/thresholds/lightMax", f)) {
      dbSetFloat(DEVICE_PATH + "/settings/thresholds/lightMax", 5500.0);
    }
  }

  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("BH1750 STARTED SUCCESSFULLY.");
  } else {
    Serial.println("BH1750 FAILED TO START.");
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED NOT FOUND");
    while (true);
  }

  screenTimer = millis();
  sensorTimer = millis();
  controlTimer = millis();

  showStartupScreen();

  Serial.println();
  printLine();
  Serial.println("              SMART PLANT MONITORING SYSTEM");
  printLine();
  Serial.println(" OLED + BH1750 + DHT22 + SOIL + PUMP + 2 BUZZERS + ML + FIREBASE");
  printLine();

  updateSensorsAndLogic();
}

// -------------------- LOOP --------------------
void loop() {
  unsigned long now = millis();

  updateOLED(latestTemp, latestHum, latestMoisturePct, latestLux, diagnosis, recommendation);

  if (now - controlTimer >= CONTROL_INTERVAL) {
    controlTimer = now;
    syncControlAndSettings();
  }

  if (!wateringActive && manualWaterRequested) {
    cooldownActive = false;
    manualWaterRequested = false;
    dbSetBool(DEVICE_PATH + "/control/manualWaterNow", false);
    activeWateringDuration = manualWateringDuration;
    startWatering(now);
    pushAlertToFirebase("info", "Pump", "Manual watering command executed",
                        "Run time: " + String((int)(manualWateringDuration / 1000)) + "s",
                        now);
    waterStressCount = 0;
    recommendation = getRecommendation(diagnosis, wateringActive, cooldownActive);
    handleBuzzerAlerts(diagnosis, latestPumpOn);
  }

  if (wateringActive && (now - wateringStartTime >= activeWateringDuration)) {
    stopWateringAndStartCooldown(now);
    recommendation = getRecommendation(diagnosis, wateringActive, cooldownActive);
  }

  if (now - sensorTimer >= SENSOR_INTERVAL) {
    sensorTimer = now;
    updateSensorsAndLogic();
  }

  delay(50);
}
