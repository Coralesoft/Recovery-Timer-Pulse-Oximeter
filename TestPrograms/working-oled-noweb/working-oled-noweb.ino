/*
  ESP32 + MAX30102 + SSD1306 OLED (128x32) — SpO₂ & pulse display
  ----------------------------------------------------------------
  - Reads Red/IR values from the MAX30102 via I²C.
  - Uses Maxim's reference SpO₂ algorithm (spo2_algorithm.h) over a rolling 100-sample window.
  - Shows smoothed SpO₂ and heart rate on a 128x32 OLED.
  - Implements simple auto-gain by nudging LED pulse amplitudes to keep IR DC level in a target band.
  - Lightweight 1 Hz compute/refresh cadence to keep UI stable and avoid flicker.

  Hardware (per your build):
    * ESP32 I²C: SDA = 21, SCL = 22
    * MAX30102 on same I²C bus
    * 0.91" SSD1306 OLED (I²C address 0x3C)

  Notes:
    * BUFLEN = 100 samples. With SAMPLE_RATE = 100 Hz, the algorithm runs over ~1 s of data.
    * Display values use a simple EMA for visual smoothing (does not affect the algorithm’s inputs).
    * FINGER_IR_THRESHOLD guards against showing junk when no finger is on the sensor.
*/

#include <Wire.h>            // ESP32 I²C bus
#include <string.h>          // for memmove() used to slide sample windows
#include "MAX30105.h"        // SparkFun MAX3010x driver (works with MAX30102)
#include "spo2_algorithm.h"  // Maxim reference implementation for HR/SpO₂

// ===== OLED (SSD1306) =====
#include <Adafruit_GFX.h>         // Graphics primitives
#include <Adafruit_SSD1306.h>     // SSD1306 driver

// Logical display size for your module
#define OLED_WIDTH  128
#define OLED_HEIGHT  32   // Set to 64 if you swap to a 128x64 panel

// Typical I²C address for 0.91" SSD1306 modules
#define OLED_ADDR 0x3C

// Construct the display object using the shared Wire bus; -1 = no reset pin
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// ===== I²C pins (ESP32 default workable pins; you’ve wired to 21/22) =====
#define I2C_SDA 21
#define I2C_SCL 22

// ===== MAX30102 configuration =====
MAX30105 sensor;                 // Driver instance for the MAX3010x family

// Sensor configuration constants
const byte LED_BRIGHTNESS = 220;   // Initial LED current (0..255). Auto-gain may change this.
const byte SAMPLE_AVG     = 16;     // On-sensor averaging: 1,2,4,8,16,32 (trade noise vs latency)
const byte LED_MODE       = 2;     // 2 = Red + IR (required for SpO₂)
const byte SAMPLE_RATE    = 100;   // Sample rate in Hz
const int  PULSE_WIDTH    = 411;   // LED pulse width in µs (longer ⇒ deeper ADC range)
const int  ADC_RANGE      = 16384; // ADC full-scale (2048..16384), depends on pulse width

// ===== Algorithm buffers (rolling window of samples) =====
const int32_t BUFLEN = 100;    // spo2_algorithm expects exactly 100 samples
uint32_t irBuf[BUFLEN];        // IR sample buffer (DC + AC)
uint32_t redBuf[BUFLEN];       // Red sample buffer (DC + AC)
int bufCount = 0;              // How many samples are currently filled (0..BUFLEN)

// Outputs from the algorithm (volatile because updated in normal loop cadence)
volatile int32_t spo2 = -1;        // Percentage (0..100) or -1 if unknown
volatile int8_t  validSpo2 = 0;    // Non-zero if spo2 value is valid
volatile int32_t heartRate = -1;   // Beats per minute or -1 if unknown
volatile int8_t  validHr = 0;      // Non-zero if heartRate value is valid
volatile uint32_t lastIR = 0;      // Latest IR raw value (used for presence/auto-gain)
volatile uint32_t lastComputeMs = 0; // Timestamp of the last algorithm compute

// Minimal IR threshold to decide “finger present” and suppress junk output
const uint32_t FINGER_IR_THRESHOLD = 15000;

// ===== Auto-gain (LED amplitude) targets =====
// Goal: Keep IR DC level within a band so the algorithm has a stable signal.
// Adjust by gently nudging LED current to avoid oscillations and visible flicker.
const uint32_t IR_TARGET_LOW  = 25000;   // If below → increase LED current a bit
const uint32_t IR_TARGET_HIGH = 90000;   // If above → decrease LED current a bit
const byte     LED_MIN        = 20;      // Hard floor for LED current
const byte     LED_MAX        = 255;     // Hard ceiling for LED current
const byte     LED_STEP       = 5;       // Increment/decrement per adjustment
byte           ledCurrent     = LED_BRIGHTNESS; // Mutable LED current used for auto-gain
uint32_t       lastGainAdjust = 0;       // Last time we changed LED current
const uint32_t GAIN_PERIOD_MS = 800;     // Only adjust at most ~1.25 Hz

// ===== Display smoothing (EMA applied to displayed numbers only) =====
// This reduces visual jitter without altering the underlying algorithm results.
int displaySpo2 = -1;          // Smoothed SpO₂ for UI (-1 means “no value yet”)
int displayHr   = -1;          // Smoothed HR for UI
const int EMA_NUM   = 3;       // Numerator of smoothing factor
const int EMA_DENOM = 10;      // Denominator → larger = heavier smoothing

// Forward declaration for the OLED update function
void renderOLED();

// ===== Sensor + OLED bring-up =====
void setupSensor() {
  // Start I²C on the pins you’re using, and run bus at 400 kHz (“fast mode”)
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  // Bring up the OLED first so we can show status messages during sensor init
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    // If the OLED isn’t detected at 0x3C, warn on serial and continue headless
    Serial.println("SSD1306 not found at 0x3C. Check wiring/address.");
  } else {
    // Small boot banner to confirm OLED is alive
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("OLED OK");
    display.display();
  }

  // Try to initialise the MAX30102 via the shared Wire bus (fast I²C)
  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    // If not found, show a helpful message on both serial and OLED (if present)
    Serial.println("MAX30102 not found. Check wiring.");
    if (display.width() > 0) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("MAX30102 not found");
      display.println("Check wiring/I2C");
      display.display();
    }
    // Halt here to avoid running the main loop without a sensor
    while (1) delay(1000);
  }

  // Apply sensor configuration (SparkFun helper configures multiple registers)
  sensor.setup(LED_BRIGHTNESS, SAMPLE_AVG, LED_MODE, SAMPLE_RATE, PULSE_WIDTH, ADC_RANGE);

  // Explicitly set pulse amplitudes (we’ll adjust these later for auto-gain)
  sensor.setPulseAmplitudeRed(LED_BRIGHTNESS);
  sensor.setPulseAmplitudeIR(LED_BRIGHTNESS);
  sensor.setPulseAmplitudeGreen(0);  // Green not used for SpO₂

  // Track current LED setting for auto-gain logic
  ledCurrent = LED_BRIGHTNESS;

  Serial.println("MAX30102 initialised.");

  // Let the user know bring-up succeeded
  if (display.width() > 0) {
    display.setCursor(0, 16);
    display.println("Sensor init OK");
    display.display();
  }
}

// Simple integer EMA (exponential moving average) for display only
static inline int emaUpdate(int current, int newVal) {
  if (current < 0) return newVal; // First sample: seed with the new value
  // EMA: current = (1-α)*current + α*new, implemented with integers
  return ( (EMA_DENOM - EMA_NUM) * current + EMA_NUM * newVal ) / EMA_DENOM;
}

// Run the SpO₂/HR algorithm once we have a full buffer of 100 samples
void computeIfReady() {
  if (bufCount < BUFLEN) return; // Not enough data yet

  // Don’t compute if there’s likely no finger present — avoids spurious results
  if (lastIR < FINGER_IR_THRESHOLD) {
    validSpo2 = 0;
    validHr = 0;
    return;
  }

  // Local temporaries to receive algorithm outputs
  int32_t s, hr;
  int8_t vs, vhr;

  // Call Maxim’s reference implementation on the rolling window
  maxim_heart_rate_and_oxygen_saturation(irBuf, BUFLEN, redBuf, &s, &vs, &hr, &vhr);

  // Publish new values (marked volatile above)
  spo2      = s;
  validSpo2 = vs;
  heartRate = hr;
  validHr   = vhr;
  lastComputeMs = millis();

  // Update smoothed UI values only when the new reading is plausible/valid
  if (validSpo2 && spo2 > 0 && spo2 <= 100) {
    displaySpo2 = emaUpdate(displaySpo2, (int)spo2);
  }
  if (validHr && heartRate > 0 && heartRate < 240) {
    displayHr = emaUpdate(displayHr, (int)heartRate);
  }
}

// Gentle auto-gain loop to keep IR DC level within a target band
void maybeAdjustAutoGain() {
  // Throttle adjustments and only try when a finger is present
  uint32_t now = millis();
  if (now - lastGainAdjust < GAIN_PERIOD_MS) return;
  if (lastIR < FINGER_IR_THRESHOLD) return;

  bool changed = false;

  // If IR DC is low, increase LED current slightly
  if (lastIR < IR_TARGET_LOW && ledCurrent < LED_MAX) {
    byte next = ledCurrent + LED_STEP;
    ledCurrent = (next > LED_MAX) ? LED_MAX : next;
    changed = true;

  // If IR DC is high, decrease LED current slightly
  } else if (lastIR > IR_TARGET_HIGH && ledCurrent > LED_MIN) {
    byte next = (ledCurrent > LED_STEP) ? ledCurrent - LED_STEP : LED_MIN;
    ledCurrent = (next < LED_MIN) ? LED_MIN : next;
    changed = true;
  }

  // Apply updated LED current to both Red and IR channels if we changed it
  if (changed) {
    sensor.setPulseAmplitudeRed(ledCurrent);
    sensor.setPulseAmplitudeIR(ledCurrent);
    // We don’t redraw OLED immediately; normal 1 Hz UI refresh is enough.
    lastGainAdjust = now;
  }
}

// ===== OLED rendering (lightweight, 1 Hz) =====
void renderOLED() {
  if (display.width() == 0) return; // If OLED wasn’t found, do nothing

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  // Line 1: Status (finger presence)
  if (lastIR >= FINGER_IR_THRESHOLD) {
    display.print("Status: ");
    display.println("Reading");
  } else {
    display.print("Status: ");
    display.println("No finger");
  }

  // Line 2: SpO₂ (smoothed)
  display.print("SpO2: ");
  if (validSpo2 && displaySpo2 > 0) {
    display.print(displaySpo2);
    display.println(" %");
  } else {
    display.println("-- %");
  }

  // Line 3: Pulse (smoothed)
  display.print("Pulse: ");
  if (validHr && displayHr > 0) {
    display.print(displayHr);
    display.println(" bpm");
  } else {
    display.println("--- bpm");
  }

  // Line 4: simple heartbeat “trail” — a single pixel marching across bottom row
  // Note: Based purely on time; step every 100 ms so it visibly moves between 1 Hz refreshes.
  int x = (millis() / 100) % OLED_WIDTH;
  display.drawPixel(x, OLED_HEIGHT - 1, SSD1306_WHITE);

  display.display(); // Push the buffer to the panel
}

// ===== Arduino lifecycle =====
void setup() {
  Serial.begin(115200); // Serial for logs/debug
  delay(200);           // Small settle
  setupSensor();        // Bring up I²C devices and configure the sensor
}

void loop() {
  // Pull samples continuously when available from the MAX30102 FIFO
  if (sensor.available()) {
    // Read the newest sample (Red & IR), then advance FIFO pointer
    uint32_t red = sensor.getRed();
    uint32_t ir  = sensor.getIR();
    sensor.nextSample();

    // Keep latest IR for presence detection and auto-gain logic
    lastIR = ir;

    if (bufCount < BUFLEN) {
      // Still filling the initial window
      redBuf[bufCount] = red;
      irBuf[bufCount]  = ir;
      bufCount++;
    } else {
      // Rolling window: shift left by 1 and append the newest sample at the end
      memmove(redBuf, redBuf + 1, (BUFLEN - 1) * sizeof(uint32_t));
      memmove(irBuf,  irBuf  + 1, (BUFLEN - 1) * sizeof(uint32_t));
      redBuf[BUFLEN - 1] = red;
      irBuf[BUFLEN - 1]  = ir;
    }
  } else {
    // Ask the driver to fetch more from the sensor’s FIFO if the library requires it
    sensor.check();
  }

  // Compute SpO₂/HR and refresh OLED roughly once per second
  static uint32_t lastCalc = 0;
  if (millis() - lastCalc >= 1000) {   // 1 Hz cadence
    computeIfReady();                  // Run algorithm if buffer is ready
    renderOLED();                      // Update UI to show latest values
    lastCalc = millis();
  }

  // Run auto-gain slightly more often than UI updates (throttled internally)
  maybeAdjustAutoGain();

  delay(2); // Tiny yield to keep the loop cooperative on the ESP32
}
