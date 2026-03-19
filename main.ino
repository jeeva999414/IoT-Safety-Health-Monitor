/*
 * ============================================================
 *  IoT-Based Emergency Safety & Health Monitoring System
 *  For Women, Elderly, and Patients
 * ============================================================
 *  Hardware:
 *    - ESP32 Dev Kit
 *    - Pulse Sensor (Heart Rate) → GPIO 34 (ADC)
 *    - DHT11 (Temp + Humidity)   → GPIO 4
 *    - GPS Module (NEO-6M)       → GPIO 16 (RX2), GPIO 17 (TX2)
 *    - Emergency Push Button     → GPIO 2
 *    - Buzzer                    → GPIO 5
 *    - Power ON/OFF Switch       → In-line with VCC
 * ============================================================
 *  Communication: Telegram Bot
 *  Libraries Required:
 *    - UniversalTelegramBot by Brian Lough
 *    - ArduinoJson
 *    - DHT sensor library by Adafruit
 *    - TinyGPS++ by Mikal Hart
 *    - WiFi (built-in ESP32)
 * ============================================================
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>

// ─── WiFi Credentials ───────────────────────────────────────
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

// ─── Telegram Bot Credentials ───────────────────────────────
#define BOT_TOKEN       "YOUR_TELEGRAM_BOT_TOKEN"
#define CHAT_ID         "YOUR_TELEGRAM_CHAT_ID"

// ─── Pin Definitions ────────────────────────────────────────
#define PULSE_SENSOR_PIN  34    // Analog pin for heart rate sensor
#define DHT_PIN           4     // Digital pin for DHT11
#define DHT_TYPE          DHT11
#define BUTTON_PIN        2     // Emergency push button (active LOW)
#define BUZZER_PIN        5     // Buzzer output

// ─── GPS Serial ─────────────────────────────────────────────
#define GPS_RX_PIN        16    // ESP32 RX2 ← GPS TX
#define GPS_TX_PIN        17    // ESP32 TX2 → GPS RX
#define GPS_BAUD          9600

// ─── Thresholds ─────────────────────────────────────────────
#define HEART_RATE_HIGH   120   // BPM — triggers auto-alert
#define HEART_RATE_LOW    45    // BPM — triggers auto-alert
#define TEMP_HIGH         38.5  // °C  — fever threshold

// ─── Timing ─────────────────────────────────────────────────
#define BUTTON_DEBOUNCE_MS   200
#define SENSOR_INTERVAL_MS   2000
#define BOT_POLL_INTERVAL_MS 1000
#define BUZZER_BEEP_MS       3000

// ─── Objects ────────────────────────────────────────────────
DHT          dht(DHT_PIN, DHT_TYPE);
TinyGPSPlus  gps;
HardwareSerial gpsSerial(2);   // UART2

WiFiClientSecure  client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// ─── State Variables ────────────────────────────────────────
bool     emergencyActive    = false;
bool     buzzerOn           = false;
unsigned long lastSensorRead = 0;
unsigned long lastBotCheck   = 0;
unsigned long buzzerStart    = 0;
int      lastButtonState     = HIGH;
unsigned long lastDebounceTime = 0;

// Sensor data globals
float    heartRate     = 0.0;
float    temperature   = 0.0;
float    humidity      = 0.0;
double   gpsLat        = 0.0;
double   gpsLng        = 0.0;
bool     gpsFixed      = false;

// ─── Function Declarations ──────────────────────────────────
void     connectWiFi();
void     readSensors();
float    readHeartRate();
void     sendTelegramAlert(String reason);
String   buildStatusMessage();
String   buildGoogleMapsLink();
void     handleTelegramCommands(int numMessages);
void     triggerEmergency(String reason);
void     stopBuzzer();
void     beepBuzzer(int times, int delayMs);

// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Serial.println("\n[BOOT] IoT Safety System Starting...");

  // Pin modes
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Sensors init
  dht.begin();
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println("[OK] Sensors initialized");

  // Startup beep
  beepBuzzer(2, 200);

  // WiFi connect
  connectWiFi();

  // Telegram SSL
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  Serial.println("[OK] System Ready. Monitoring started.");
  bot.sendMessage(CHAT_ID, "✅ *Safety Device Online*\nMonitoring started. Send /help for commands.", "Markdown");
}

// ════════════════════════════════════════════════════════════
void loop() {
  unsigned long now = millis();

  // ── Read GPS continuously ──────────────────────────────
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
  if (gps.location.isValid()) {
    gpsLat   = gps.location.lat();
    gpsLng   = gps.location.lng();
    gpsFixed = true;
  }

  // ── Read sensors every SENSOR_INTERVAL_MS ─────────────
  if (now - lastSensorRead >= SENSOR_INTERVAL_MS) {
    lastSensorRead = now;
    readSensors();

    // Auto-alert on abnormal readings
    if (heartRate > HEART_RATE_HIGH) {
      triggerEmergency("⚠️ HIGH HEART RATE DETECTED: " + String(heartRate, 0) + " BPM");
    } else if (heartRate > 10 && heartRate < HEART_RATE_LOW) {
      triggerEmergency("⚠️ LOW HEART RATE DETECTED: " + String(heartRate, 0) + " BPM");
    }
    if (temperature > TEMP_HIGH) {
      triggerEmergency("🌡️ HIGH TEMPERATURE ALERT: " + String(temperature, 1) + " °C");
    }
  }

  // ── Emergency Button (debounced) ──────────────────────
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) {
    lastDebounceTime = now;
  }
  if ((now - lastDebounceTime) > BUTTON_DEBOUNCE_MS) {
    if (reading == LOW && lastButtonState == HIGH) {
      Serial.println("[ALERT] Emergency button pressed!");
      triggerEmergency("🆘 EMERGENCY BUTTON PRESSED");
    }
  }
  lastButtonState = reading;

  // ── Auto-stop buzzer after BUZZER_BEEP_MS ─────────────
  if (buzzerOn && (now - buzzerStart >= BUZZER_BEEP_MS)) {
    stopBuzzer();
  }

  // ── Telegram Bot polling ──────────────────────────────
  if (now - lastBotCheck >= BOT_POLL_INTERVAL_MS) {
    lastBotCheck = now;
    int numMsg = bot.getUpdates(bot.last_message_received + 1);
    if (numMsg > 0) handleTelegramCommands(numMsg);
  }
}

// ════════════════════════════════════════════════════════════
void connectWiFi() {
  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected! IP: " + WiFi.localIP().toString());
    beepBuzzer(1, 300);
  } else {
    Serial.println("\n[WiFi] FAILED. Running in offline mode.");
  }
}

// ════════════════════════════════════════════════════════════
void readSensors() {
  // DHT11 — Temperature & Humidity
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temperature = t;
  if (!isnan(h)) humidity    = h;

  // Pulse Sensor — Heart Rate estimation
  heartRate = readHeartRate();

  Serial.printf("[Sensors] HR: %.0f bpm | Temp: %.1f°C | Humidity: %.0f%%\n",
                heartRate, temperature, humidity);
}

// ════════════════════════════════════════════════════════════
float readHeartRate() {
  /*
   * Simple peak-detection method for the Pulse Sensor.
   * Samples the analog signal for ~2 seconds and counts peaks
   * to estimate BPM. For production, use the PulseSensor Playground
   * library for higher accuracy.
   */
  const int   SAMPLE_COUNT   = 100;
  const int   SAMPLE_DELAY   = 20;   // ms between samples
  const int   THRESHOLD      = 550;  // ADC threshold for peak
  const int   MIN_PEAK_DIST  = 10;   // min samples between peaks

  int   peakCount     = 0;
  bool  inPeak        = false;
  int   samplesSince  = MIN_PEAK_DIST + 1;
  int   rawVal        = 0;

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    rawVal = analogRead(PULSE_SENSOR_PIN);
    samplesSince++;

    if (rawVal > THRESHOLD && !inPeak && samplesSince > MIN_PEAK_DIST) {
      inPeak = true;
      peakCount++;
      samplesSince = 0;
    }
    if (rawVal < THRESHOLD - 50) {
      inPeak = false;
    }
    delay(SAMPLE_DELAY);
  }

  // Time window = SAMPLE_COUNT * SAMPLE_DELAY ms
  float windowSec = (SAMPLE_COUNT * SAMPLE_DELAY) / 1000.0;
  float bpm       = (peakCount / windowSec) * 60.0;

  return (bpm > 200 || bpm < 1) ? 0.0 : bpm;
}

// ════════════════════════════════════════════════════════════
void triggerEmergency(String reason) {
  if (emergencyActive) return;  // prevent duplicate alerts
  emergencyActive = true;

  Serial.println("[EMERGENCY] " + reason);

  // Start buzzer
  digitalWrite(BUZZER_PIN, HIGH);
  buzzerOn    = true;
  buzzerStart = millis();

  // Send Telegram alert
  sendTelegramAlert(reason);

  // Reset after 10 sec to allow re-trigger
  delay(10000);
  emergencyActive = false;
}

// ════════════════════════════════════════════════════════════
void sendTelegramAlert(String reason) {
  String msg = "🚨 *EMERGENCY ALERT*\n";
  msg += "━━━━━━━━━━━━━━━━━━\n";
  msg += "*Reason:* " + reason + "\n\n";
  msg += "*📊 Live Health Data:*\n";
  msg += "❤️ Heart Rate: " + String(heartRate, 0) + " BPM\n";
  msg += "🌡️ Temperature: " + String(temperature, 1) + " °C\n";
  msg += "💧 Humidity: " + String(humidity, 0) + " %\n\n";
  msg += "*📍 Location:*\n";

  if (gpsFixed) {
    msg += "Lat: " + String(gpsLat, 6) + "\n";
    msg += "Lng: " + String(gpsLng, 6) + "\n";
    msg += buildGoogleMapsLink() + "\n";
  } else {
    msg += "⚠️ GPS signal not acquired yet.\n";
  }

  msg += "━━━━━━━━━━━━━━━━━━\n";
  msg += "_Please respond immediately!_";

  bot.sendMessage(CHAT_ID, msg, "Markdown");
  Serial.println("[Telegram] Emergency alert sent.");
}

// ════════════════════════════════════════════════════════════
String buildStatusMessage() {
  String msg = "📊 *Live Health & Location Status*\n";
  msg += "━━━━━━━━━━━━━━━━━━\n";
  msg += "❤️ Heart Rate: " + String(heartRate, 0) + " BPM\n";
  msg += "🌡️ Temperature: " + String(temperature, 1) + " °C\n";
  msg += "💧 Humidity: " + String(humidity, 0) + " %\n\n";
  msg += "*📍 Location:*\n";
  if (gpsFixed) {
    msg += "Lat: " + String(gpsLat, 6) + "\n";
    msg += "Lng: " + String(gpsLng, 6) + "\n";
    msg += buildGoogleMapsLink();
  } else {
    msg += "⚠️ GPS signal not fixed yet.";
  }
  return msg;
}

// ════════════════════════════════════════════════════════════
String buildGoogleMapsLink() {
  return "🗺️ [Open in Maps](https://maps.google.com/?q=" +
         String(gpsLat, 6) + "," + String(gpsLng, 6) + ")";
}

// ════════════════════════════════════════════════════════════
void handleTelegramCommands(int numMessages) {
  for (int i = 0; i < numMessages; i++) {
    String text   = bot.messages[i].text;
    String chatId = bot.messages[i].chat_id;

    Serial.println("[Telegram] Command received: " + text);

    if (text == "/start" || text == "/help") {
      String help = "🛡️ *Safety Device Commands*\n\n";
      help += "/data — Get live health & location\n";
      help += "/status — Device status\n";
      help += "/buzzer\\_off — Stop buzzer alarm\n";
      help += "/test — Send a test alert\n";
      help += "/help — Show this menu\n";
      bot.sendMessage(chatId, help, "Markdown");
    }
    else if (text == "/data") {
      bot.sendMessage(chatId, buildStatusMessage(), "Markdown");
    }
    else if (text == "/status") {
      String s = "✅ *Device Status: Online*\n";
      s += "WiFi: " + WiFi.localIP().toString() + "\n";
      s += "GPS: " + String(gpsFixed ? "Fixed ✅" : "Searching... ⏳") + "\n";
      s += "Uptime: " + String(millis() / 60000) + " min";
      bot.sendMessage(chatId, s, "Markdown");
    }
    else if (text == "/buzzer_off") {
      stopBuzzer();
      bot.sendMessage(chatId, "🔕 Buzzer stopped.", "Markdown");
    }
    else if (text == "/test") {
      bot.sendMessage(chatId, "🧪 *Test Alert Triggered*\n" + buildStatusMessage(), "Markdown");
    }
    else {
      bot.sendMessage(chatId, "❓ Unknown command. Send /help for commands.", "");
    }
  }
}

// ════════════════════════════════════════════════════════════
void stopBuzzer() {
  digitalWrite(BUZZER_PIN, LOW);
  buzzerOn = false;
  Serial.println("[Buzzer] Stopped.");
}

// ════════════════════════════════════════════════════════════
void beepBuzzer(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(delayMs);
    digitalWrite(BUZZER_PIN, LOW);
    delay(delayMs);
  }
}
