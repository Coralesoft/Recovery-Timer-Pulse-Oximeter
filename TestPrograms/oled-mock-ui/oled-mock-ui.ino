#include <Wire.h>                 // I²C bus (ESP32: SDA=21, SCL=22 by default)
#include <Adafruit_GFX.h>         // Graphics primitives (text, shapes)
#include <Adafruit_SSD1306.h>     // Driver for SSD1306 OLED displays

// ---- OLED setup ----
#define SCREEN_WIDTH 128          // Pixels (your 0.91" module is 128x32)
#define SCREEN_HEIGHT 32
#define OLED_ADDR 0x3C            // Common I²C address for these OLEDs (0x3D on some)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
// 4th arg is reset pin; -1 = not used (module auto-resets over I²C)

// ---- Buzzer (optional) ----
const int BUZZER = 19;            // Active buzzer on GPIO19 (3.3 V). LOW = off, HIGH = on

// ---- Mock vitals (until MAX30102 is wired) ----
int hr = 96, spo2 = 97;           // Start values for the “fake” readings
const int HR_TARGET = 80;         // Beep when HR drops to/below this
const int SPO2_MIN  = 95;         // …and SpO₂ is at/above this
bool targetReached = false;       // Simple latch so we beep once
unsigned long nextUpdate = 0;     // Millis timestamp for next UI update

// Short helper to make the buzzer chirp
void beep(int ms) {
  digitalWrite(BUZZER, HIGH);
  delay(ms);
  digitalWrite(BUZZER, LOW);
}

// Draw a two-line banner (used at startup)
void drawHeader(const char* line1, const char* line2) {
  display.clearDisplay();                 // Clear the whole framebuffer
  display.setTextSize(1);                 // 6x8 px font
  display.setTextColor(SSD1306_WHITE);    // Pixels on = white
  display.setCursor(0, 0);  display.println(line1);
  display.setCursor(0,10);  display.println(line2);
  display.display();                       // Push framebuffer to the panel
}

// Compact UI: header + two lines of values, right-aligned
void drawVitalsSmall(int s, int h) {
  // Clear the full area we draw into (avoids leftover pixels)
  display.fillRect(0, 0, 128, 32, SSD1306_BLACK);

  // Header
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Reading Finger");

  // Line 1: Oxy (right-aligned at x=120 so digits don’t “jitter”)
  display.setCursor(0, 12);
  display.print("Oxy:");
  char buf1[8];
  snprintf(buf1, sizeof(buf1), "%3d", s); // pad to 3 chars
  int16_t x1, y1; uint16_t w, hgt;
  display.getTextBounds(buf1, 0, 0, &x1, &y1, &w, &hgt);
  display.setCursor(120 - w, 12);
  display.print(buf1);

  // Line 2: HR (also right-aligned)
  display.setCursor(0, 22);
  display.print("HR:");
  char buf2[8];
  snprintf(buf2, sizeof(buf2), "%3d", h);
  display.getTextBounds(buf2, 0, 0, &x1, &y1, &w, &hgt);
  display.setCursor(120 - w, 22);
  display.print(buf2);

  display.display(); // Update the screen
}

// Very small random walk so the UI looks “alive”
void updateMock() {
  hr   += random(-1, 2);          // -1, 0, or +1
  spo2 += random(-1, 2);
  hr   = constrain(hr,   55, 120);
  spo2 = constrain(spo2, 90,  99);
}

// Beep once when both targets are met; add hysteresis to prevent chatter
void checkTarget() {
  bool ok = (hr <= HR_TARGET) && (spo2 >= SPO2_MIN);
  if (ok && !targetReached) {     // first time we cross the target
    beep(120);
    targetReached = true;
  }
  // Reset the latch only after we move away from the target by a margin
  if (!ok && (hr > HR_TARGET + 2 || spo2 < SPO2_MIN - 1)) {
    targetReached = false;
  }
}

void setup() {
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);      // make sure it’s silent at boot

  Wire.begin(21, 22);             // ESP32 I²C pins: SDA=21, SCL=22
  // Start the OLED; if not found, stop here (common addresses: 0x3C/0x3D)
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true) delay(1000);     // “fail fast” if display missing
  }

  randomSeed(esp_random());       // Good entropy source on ESP32

  // Startup splash
  drawHeader("Max's Oxy Meter", "OLED Mock UI v1");
  delay(1200);
}

void loop() {
  unsigned long now = millis();
  if (now >= nextUpdate) {
    updateMock();                 // refresh fake sensor values
    drawVitalsSmall(spo2, hr);    // redraw UI
    checkTarget();                // beep if we’ve hit the target
    nextUpdate = now + 400;       // ~2.5 Hz refresh rate
  }
}
