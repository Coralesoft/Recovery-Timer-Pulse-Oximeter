#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <string.h>          // for memmove
#include "MAX30105.h"
#include "spo2_algorithm.h"

// ===== OLED (SSD1306) =====
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define OLED_WIDTH   128
#define OLED_HEIGHT   32   // change to 64 if you have a 128x64 module
#define OLED_ADDR   0x3C   // most 0.91" modules use 0x3C
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1); // no reset pin

// ===== Wi-Fi/LAN settings (192.168.18.0/24) =====
const char* WIFI_SSID = "ZicoNet";
const char* WIFI_PASS = "Samoan88";
const char* MDNS_HOST = "oximeter";      // http://oximeter.local

#define USE_STATIC_IP 1
IPAddress STA_IP(192,168,18,28);
IPAddress STA_GW(192,168,18,1);
IPAddress STA_MASK(255,255,255,0);
IPAddress STA_DNS1(192,168,18,1);
IPAddress STA_DNS2(1,1,1,1);

// ===== I2C pins =====
#define I2C_SDA 21
#define I2C_SCL 22

// ===== MAX30102 config =====
MAX30105 sensor;
const byte LED_BRIGHTNESS = 60;   // 0..255
const byte SAMPLE_AVG     = 4;    // 1,2,4,8,16,32
const byte LED_MODE       = 2;    // 2 = Red + IR (SpO2)
const byte SAMPLE_RATE    = 100;  // Hz
const int  PULSE_WIDTH    = 411;  // us
const int  ADC_RANGE      = 16384;// 2048..16384

// ===== Algorithm buffers =====
const int32_t BUFLEN = 100;       // needs 100 samples
uint32_t irBuf[BUFLEN];
uint32_t redBuf[BUFLEN];
int bufCount = 0;

volatile int32_t spo2 = -1;
volatile int8_t  validSpo2 = 0;
volatile int32_t heartRate = -1;
volatile int8_t  validHr = 0;
volatile uint32_t lastIR = 0;
volatile uint32_t lastComputeMs = 0;

const uint32_t FINGER_IR_THRESHOLD = 50000;

// ===== Web server =====
WebServer server(80);

// Minimal HTML page (auto-polls /data every 500 ms)
const char* HTML = R"HTML(
<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Oximeter</title>
<style>
 body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif;margin:24px}
 .big{font-size:42px;font-weight:700}
 .muted{color:#666}
 .ok{color:#0a0}
 .warn{color:#a60}
 .bad{color:#c00}
 .card{max-width:460px;border:1px solid #ddd;border-radius:12px;padding:18px;box-shadow:0 1px 4px rgba(0,0,0,.06)}
</style></head><body>
<h2>ESP32 Oximeter</h2>
<div class="card">
  <div id="status" class="muted">Connecting…</div>
  <div class="big" id="spo2">SpO₂: --%</div>
  <div class="big" id="hr">Pulse: --- BPM</div>
  <div class="muted" id="ts"></div>
</div>
<script>
async function poll(){
  try{
    const r = await fetch('/data');
    const j = await r.json();
    const s = document.getElementById('status');
    const o = document.getElementById('spo2');
    const h = document.getElementById('hr');
    const t = document.getElementById('ts');
    const nf = j.finger === false;
    s.textContent = nf ? 'No finger detected' : 'Reading…';
    s.className = nf ? 'bad' : 'ok';
    o.textContent = 'SpO\u2082: ' + (j.validSpo2 ? j.spo2.toString().padStart(3,' ') : '--') + '%';
    h.textContent = 'Pulse: ' + (j.validHr ? j.hr.toString().padStart(3,' ') : '---') + ' BPM';
    t.textContent = 'Updated: ' + new Date(j.ts).toLocaleTimeString();
  }catch(e){
    const s = document.getElementById('status');
    s.textContent = 'Disconnected';
    s.className = 'warn';
  }
}
setInterval(poll, 500);
poll();
</script>
</body></html>
)HTML";

// ===== HTTP handlers =====
void handleRoot() {
  server.send(200, "text/html", HTML);
}

void handleData() {
  char buf[160];
  bool finger = (lastIR >= FINGER_IR_THRESHOLD);
  snprintf(buf, sizeof(buf),
    "{\"spo2\":%ld,\"validSpo2\":%d,\"hr\":%ld,\"validHr\":%d,"
    "\"finger\":%s,\"ts\":%lu}",
    (long)spo2, (int)validSpo2, (long)heartRate, (int)validHr,
    finger ? "true":"false",
    (unsigned long)millis()
  );
  server.send(200, "application/json", buf);
}

void handleInfo() {
  char b[256];
  bool sta = (WiFi.getMode() & WIFI_MODE_STA);
  IPAddress ip = sta ? WiFi.localIP() : WiFi.softAPIP();
  snprintf(b, sizeof(b),
    "{\"mode\":\"%s\",\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"mdns\":\"%s.local\"}",
    (sta && WiFi.isConnected()) ? "STA" : "AP",
    (sta && WiFi.isConnected()) ? WiFi.SSID().c_str() : "Oximeter-AP",
    ip.toString().c_str(),
    (sta && WiFi.isConnected()) ? WiFi.RSSI() : 0,
    MDNS_HOST
  );
  server.send(200, "application/json", b);
}

// ===== Network bring-up =====
void wifiConnectOrAP() {
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(MDNS_HOST);

#if USE_STATIC_IP
  if (!WiFi.config(STA_IP, STA_GW, STA_MASK, STA_DNS1, STA_DNS2)) {
    Serial.println("WiFi.config() failed, continuing anyway…");
  }
#endif

  Serial.print("Connecting to "); Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    Serial.print("RSSI: "); Serial.println(WiFi.RSSI());
    if (MDNS.begin(MDNS_HOST)) {
      MDNS.addService("http", "tcp", 80);
      Serial.print("mDNS: http://"); Serial.print(MDNS_HOST); Serial.println(".local");
    } else {
      Serial.println("mDNS failed to start");
    }
  } else {
    Serial.println("Wi-Fi failed → starting AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Oximeter-AP");  // open AP
    Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
    if (MDNS.begin(MDNS_HOST)) MDNS.addService("http", "tcp", 80);
  }
}

// ===== Sensor =====
void setupSensor() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  // OLED first (so we can show sensor status on-screen)
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 not found at 0x3C. Check wiring/address.");
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println("OLED OK");
    display.display();
  }

  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 not found. Check wiring.");
    if (display.width() > 0) {
      display.clearDisplay();
      display.setCursor(0,0);
      display.println("MAX30102 not found");
      display.println("Check wiring/I2C");
      display.display();
    }
    while (1) delay(1000);
  }
  sensor.setup(LED_BRIGHTNESS, SAMPLE_AVG, LED_MODE, SAMPLE_RATE, PULSE_WIDTH, ADC_RANGE);
  sensor.setPulseAmplitudeRed(LED_BRIGHTNESS);
  sensor.setPulseAmplitudeIR(LED_BRIGHTNESS);
  sensor.setPulseAmplitudeGreen(0); // not used
  Serial.println("MAX30102 initialised.");

  if (display.width() > 0) {
    display.setCursor(0,16);
    display.println("Sensor init OK");
    display.display();
  }
}

void computeIfReady() {
  if (bufCount < BUFLEN) return;

  // "No finger" guard using last IR
  if (lastIR < FINGER_IR_THRESHOLD) {
    validSpo2 = 0; validHr = 0;
    return;
  }

  int32_t s, hr; int8_t vs, vhr;
  maxim_heart_rate_and_oxygen_saturation(
    irBuf, BUFLEN, redBuf, &s, &vs, &hr, &vhr
  );
  spo2 = s; validSpo2 = vs;
  heartRate = hr; validHr = vhr;
  lastComputeMs = millis();
}

// ===== OLED rendering =====
void renderOLED() {
  if (display.width() == 0) return; // OLED not initialised

  display.clearDisplay();

  // Status line (finger/no finger)
  bool finger = (lastIR >= FINGER_IR_THRESHOLD);
  display.setTextSize(1);
  display.setCursor(0, 0);
  if (!finger) {
    display.println("No finger");
  } else {
    display.println("Reading...");
  }

  // SpO2 line
  display.setTextSize(2);
  display.setCursor(0, 10);
  if (validSpo2) {
    display.print("SpO2 ");
    display.print((int)spo2);
    display.print('%');
  } else {
    display.print("SpO2 --%");
  }

  // HR right-aligned (fits 128x32 nicely)
  display.setCursor(0, 26);
  display.setTextSize(1);
  if (validHr) {
    display.print("Pulse ");
    display.print((int)heartRate);
    display.print(" bpm");
  } else {
    display.print("Pulse --- bpm");
  }

  // Tiny heartbeat blip based on time (visual activity)
  int x = (millis() / 120) % OLED_WIDTH;
  display.drawPixel(x, OLED_HEIGHT - 1, SSD1306_WHITE);

  display.display();
}

// ===== Arduino lifecycle =====
void setup() {
  Serial.begin(115200);
  delay(200);
  setupSensor();
  wifiConnectOrAP();

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/info", handleInfo);
  server.onNotFound([](){
    server.send(404, "text/plain", "Not found");
  });
  server.begin();
}

void loop() {
  // Pull samples from FIFO
  if (sensor.available()) {
    uint32_t red = sensor.getRed();
    uint32_t ir  = sensor.getIR();
    sensor.nextSample();
    lastIR = ir;

    if (bufCount < BUFLEN) {
      redBuf[bufCount] = red;
      irBuf[bufCount]  = ir;
      bufCount++;
    } else {
      // Slide by 1 to keep ~100-sample window
      memmove(redBuf, redBuf + 1, (BUFLEN - 1) * sizeof(uint32_t));
      memmove(irBuf,  irBuf  + 1, (BUFLEN - 1) * sizeof(uint32_t));
      redBuf[BUFLEN - 1] = red;
      irBuf[BUFLEN - 1]  = ir;
    }
  } else {
    sensor.check(); // load more samples from sensor FIFO
  }

  static uint32_t lastCalc = 0;
  if (millis() - lastCalc > 200) { // compute ~5 Hz
    computeIfReady();
    lastCalc = millis();
  }

  static uint32_t lastOLED = 0;
  if (millis() - lastOLED > 150) { // refresh OLED ~6–7 Hz
    renderOLED();
    lastOLED = millis();
  }

  server.handleClient();
  delay(2); // small yield
}
