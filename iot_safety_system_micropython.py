# ============================================================
#  IoT-Based Emergency Safety & Health Monitoring System
#  MicroPython Code for ESP32
# ============================================================
#  Hardware:
#    - ESP32 Dev Kit
#    - Pulse Sensor (Heart Rate) → GPIO 34 (ADC)
#    - DHT11 (Temp + Humidity)   → GPIO 4
#    - GPS Module (NEO-6M)       → UART1 TX=17, RX=16
#    - Emergency Push Button     → GPIO 2
#    - Buzzer                    → GPIO 5
# ============================================================
#  MicroPython Libraries needed (upload to ESP32 via Thonny):
#    - dht.py        (built-in MicroPython)
#    - urequests.py  (upload manually)
#    - ujson.py      (built-in MicroPython)
# ============================================================

import machine
import time
import network
import urequests
import ujson
import dht
from machine import Pin, ADC, UART

# ─── WiFi Credentials ───────────────────────────────────────
WIFI_SSID     = "YOUR_WIFI_SSID"
WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"

# ─── Telegram Bot Credentials ───────────────────────────────
BOT_TOKEN = "YOUR_TELEGRAM_BOT_TOKEN"
CHAT_ID   = "YOUR_TELEGRAM_CHAT_ID"
TELEGRAM_URL = "https://api.telegram.org/bot" + BOT_TOKEN

# ─── Pin Definitions ────────────────────────────────────────
PULSE_PIN   = 34   # ADC - Pulse sensor signal
DHT_PIN     = 4    # Digital - DHT11 data
BUTTON_PIN  = 2    # Digital - Emergency button (active LOW)
BUZZER_PIN  = 5    # Digital - Buzzer output

# ─── Thresholds ─────────────────────────────────────────────
HEART_RATE_HIGH = 120   # BPM
HEART_RATE_LOW  = 45    # BPM
TEMP_HIGH       = 38.5  # Celsius

# ─── Hardware Init ──────────────────────────────────────────
pulse_sensor = ADC(Pin(PULSE_PIN))
pulse_sensor.atten(ADC.ATTN_11DB)       # Full 0-3.3V range
pulse_sensor.width(ADC.WIDTH_12BIT)     # 12-bit resolution (0-4095)

dht_sensor = dht.DHT11(Pin(DHT_PIN))

button = Pin(BUTTON_PIN, Pin.IN, Pin.PULL_UP)

buzzer = Pin(BUZZER_PIN, Pin.OUT)
buzzer.value(0)

gps_uart = UART(1, baudrate=9600, tx=17, rx=16)

# ─── Global State ───────────────────────────────────────────
heart_rate  = 0.0
temperature = 0.0
humidity    = 0.0
gps_lat     = 0.0
gps_lng     = 0.0
gps_fixed   = False
last_update_id = 0

# ════════════════════════════════════════════════════════════
def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    if not wlan.isconnected():
        print("[WiFi] Connecting to", WIFI_SSID)
        wlan.connect(WIFI_SSID, WIFI_PASSWORD)
        attempts = 0
        while not wlan.isconnected() and attempts < 20:
            time.sleep(0.5)
            print(".", end="")
            attempts += 1
    if wlan.isconnected():
        print("\n[WiFi] Connected! IP:", wlan.ifconfig()[0])
        beep_buzzer(1, 300)
        return True
    else:
        print("\n[WiFi] Failed to connect.")
        return False

# ════════════════════════════════════════════════════════════
def read_dht():
    global temperature, humidity
    try:
        dht_sensor.measure()
        temperature = dht_sensor.temperature()
        humidity    = dht_sensor.humidity()
        print("[DHT11] Temp: {}C  Humidity: {}%".format(temperature, humidity))
    except Exception as e:
        print("[DHT11] Error:", e)

# ════════════════════════════════════════════════════════════
def read_heart_rate():
    """
    Simple peak detection to estimate BPM from Pulse Sensor.
    Samples for ~2 seconds and counts peaks above threshold.
    """
    global heart_rate
    SAMPLES     = 100
    DELAY_MS    = 20
    THRESHOLD   = 2500   # ADC threshold (0-4095 for 12-bit)
    MIN_DIST    = 10     # min samples between peaks

    peak_count  = 0
    in_peak     = False
    since_peak  = MIN_DIST + 1

    for i in range(SAMPLES):
        val = pulse_sensor.read()
        since_peak += 1

        if val > THRESHOLD and not in_peak and since_peak > MIN_DIST:
            in_peak    = True
            peak_count += 1
            since_peak = 0
        if val < (THRESHOLD - 200):
            in_peak = False
        time.sleep_ms(DELAY_MS)

    window_sec = (SAMPLES * DELAY_MS) / 1000.0
    bpm = (peak_count / window_sec) * 60.0

    if bpm < 1 or bpm > 200:
        heart_rate = 0.0
    else:
        heart_rate = round(bpm, 1)

    print("[Pulse] Heart Rate: {} BPM".format(heart_rate))

# ════════════════════════════════════════════════════════════
def read_gps():
    """
    Parse NMEA GPRMC sentence from GPS module.
    Updates global gps_lat, gps_lng, gps_fixed.
    """
    global gps_lat, gps_lng, gps_fixed

    if gps_uart.any():
        try:
            line = gps_uart.readline()
            if line:
                sentence = line.decode('utf-8', 'ignore').strip()
                if sentence.startswith('$GPRMC') or sentence.startswith('$GNRMC'):
                    parts = sentence.split(',')
                    if len(parts) >= 7 and parts[2] == 'A':
                        # Valid fix
                        raw_lat = parts[3]
                        lat_dir = parts[4]
                        raw_lng = parts[5]
                        lng_dir = parts[6]

                        lat_deg = float(raw_lat[:2])
                        lat_min = float(raw_lat[2:])
                        lat = lat_deg + lat_min / 60.0
                        if lat_dir == 'S':
                            lat = -lat

                        lng_deg = float(raw_lng[:3])
                        lng_min = float(raw_lng[3:])
                        lng = lng_deg + lng_min / 60.0
                        if lng_dir == 'W':
                            lng = -lng

                        gps_lat   = round(lat, 6)
                        gps_lng   = round(lng, 6)
                        gps_fixed = True
                        print("[GPS] Lat: {}  Lng: {}".format(gps_lat, gps_lng))
        except Exception as e:
            print("[GPS] Parse error:", e)

# ════════════════════════════════════════════════════════════
def build_maps_link():
    return "https://maps.google.com/?q={},{}".format(gps_lat, gps_lng)

# ════════════════════════════════════════════════════════════
def send_telegram(message):
    try:
        url  = TELEGRAM_URL + "/sendMessage"
        data = {
            "chat_id":    CHAT_ID,
            "text":       message,
            "parse_mode": "Markdown"
        }
        response = urequests.post(
            url,
            data=ujson.dumps(data),
            headers={"Content-Type": "application/json"}
        )
        response.close()
        print("[Telegram] Message sent.")
    except Exception as e:
        print("[Telegram] Error:", e)

# ════════════════════════════════════════════════════════════
def build_alert_message(reason):
    msg  = "*EMERGENCY ALERT*\n"
    msg += "Reason: {}\n\n".format(reason)
    msg += "*Live Health Data:*\n"
    msg += "Heart Rate: {} BPM\n".format(heart_rate)
    msg += "Temperature: {} C\n".format(temperature)
    msg += "Humidity: {} %\n\n".format(humidity)
    msg += "*Location:*\n"
    if gps_fixed:
        msg += "Lat: {}  Lng: {}\n".format(gps_lat, gps_lng)
        msg += build_maps_link()
    else:
        msg += "GPS signal not acquired yet."
    return msg

# ════════════════════════════════════════════════════════════
def build_status_message():
    msg  = "*Live Health and Location Status*\n"
    msg += "Heart Rate: {} BPM\n".format(heart_rate)
    msg += "Temperature: {} C\n".format(temperature)
    msg += "Humidity: {} %\n\n".format(humidity)
    if gps_fixed:
        msg += "Lat: {}  Lng: {}\n".format(gps_lat, gps_lng)
        msg += build_maps_link()
    else:
        msg += "GPS not fixed yet."
    return msg

# ════════════════════════════════════════════════════════════
def trigger_emergency(reason):
    print("[EMERGENCY]", reason)
    buzzer.value(1)
    send_telegram(build_alert_message(reason))
    time.sleep(3)
    buzzer.value(0)

# ════════════════════════════════════════════════════════════
def beep_buzzer(times, delay_ms):
    for _ in range(times):
        buzzer.value(1)
        time.sleep_ms(delay_ms)
        buzzer.value(0)
        time.sleep_ms(delay_ms)

# ════════════════════════════════════════════════════════════
def check_telegram_commands():
    """
    Poll Telegram for new messages and respond to commands.
    """
    global last_update_id
    try:
        url      = TELEGRAM_URL + "/getUpdates?offset={}&timeout=1".format(last_update_id + 1)
        response = urequests.get(url)
        data     = ujson.loads(response.text)
        response.close()

        if data.get("ok") and data.get("result"):
            for update in data["result"]:
                last_update_id = update["update_id"]
                message = update.get("message", {})
                text    = message.get("text", "")
                chat_id = str(message.get("chat", {}).get("id", ""))

                print("[Telegram] Command:", text)

                if text in ["/start", "/help"]:
                    reply  = "*Safety Device Commands*\n\n"
                    reply += "/data - Get live health and location\n"
                    reply += "/status - Device status\n"
                    reply += "/test - Send a test alert"
                    send_telegram(reply)

                elif text == "/data":
                    send_telegram(build_status_message())

                elif text == "/status":
                    reply  = "*Device Status: Online*\n"
                    reply += "GPS: {}\n".format("Fixed" if gps_fixed else "Searching...")
                    send_telegram(reply)

                elif text == "/test":
                    send_telegram("*Test Alert*\n" + build_status_message())

    except Exception as e:
        print("[Telegram Poll] Error:", e)

# ════════════════════════════════════════════════════════════
# MAIN
# ════════════════════════════════════════════════════════════
def main():
    print("\n[BOOT] IoT Safety System - MicroPython")
    print("[BOOT] Initializing...")

    beep_buzzer(2, 200)
    connect_wifi()

    send_telegram("*Safety Device Online*\nMonitoring started. Send /help for commands.")
    print("[OK] System ready. Starting main loop...\n")

    sensor_timer  = 0
    bot_timer     = 0
    last_btn      = 1
    emergency_cooldown = 0

    while True:
        now = time.ticks_ms()

        # ── Read GPS continuously ──────────────────────────
        read_gps()

        # ── Read sensors every 2 seconds ──────────────────
        if time.ticks_diff(now, sensor_timer) >= 2000:
            sensor_timer = now
            read_dht()
            read_heart_rate()

            # Auto-alert on abnormal readings
            cooldown_ok = time.ticks_diff(now, emergency_cooldown) > 10000
            if cooldown_ok:
                if heart_rate > HEART_RATE_HIGH:
                    trigger_emergency("HIGH HEART RATE: {} BPM".format(heart_rate))
                    emergency_cooldown = now
                elif 0 < heart_rate < HEART_RATE_LOW:
                    trigger_emergency("LOW HEART RATE: {} BPM".format(heart_rate))
                    emergency_cooldown = now
                elif temperature > TEMP_HIGH:
                    trigger_emergency("HIGH TEMPERATURE: {} C".format(temperature))
                    emergency_cooldown = now

        # ── Emergency Button (debounced) ──────────────────
        btn_state = button.value()
        if btn_state == 0 and last_btn == 1:
            cooldown_ok = time.ticks_diff(now, emergency_cooldown) > 10000
            if cooldown_ok:
                print("[BUTTON] Emergency button pressed!")
                trigger_emergency("EMERGENCY BUTTON PRESSED")
                emergency_cooldown = now
        last_btn = btn_state

        # ── Poll Telegram every 1 second ──────────────────
        if time.ticks_diff(now, bot_timer) >= 1000:
            bot_timer = now
            check_telegram_commands()

        time.sleep_ms(50)

# ════════════════════════════════════════════════════════════
main()
