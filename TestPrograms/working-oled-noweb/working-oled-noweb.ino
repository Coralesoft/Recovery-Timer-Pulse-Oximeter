#include <Wire.h>
#include <string.h>  // for memmove
#include "MAX30105.h"
#include "spo2_algorithm.h"

// ===== OLED (SSD1306) =====
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define OLED_WIDTH 128
#define OLED_HEIGHT 32   // change to 64 if you have a 128x64 module
#define OLED_ADDR 0x3C   // most 0.91" modules use 0x3C
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);  // no reset pin

// ===== I2C pins =====
#define I2C_SDA 21
#define I2C_SCL 22

// ===== MAX30102 config =====
MAX30105 sensor;
const byte LED_BRIGHTNESS = 220;   // 0..255 (starting point; auto-gain may adjust)
const byte SAMPLE_AVG     = 8;     // 1,2,4,8,16,32
const byte LED_MODE       = 2;     // 2 = Red + IR (SpO2)
const byte SAMPLE_RATE    = 100;   // Hz
const int  PULSE_WIDTH    = 411;   // us
const int  ADC_RANGE      = 16384; // 2048..16384

// ===== Algorithm buffers =====
const int32_t BUFLEN = 100;  // needs 100 samples
uint32_t irBuf[BUFLEN];
uint32_t redBuf[BUFLEN];
int bufCount = 0;

volatile int32_t spo2 = -1;
volatile int8_t  validSpo2 = 0;
volatile int32_t heartRate = -1;
volatile int8_t  validHr = 0;
volatile uint32_t lastIR = 0;
volatile uint32_t lastComputeMs = 0;

const uint32_t FINGER_IR_THRESHOLD = 15000;

// ===== Auto-gain (LED amplitude) targets =====
// Keep IR DC level in this band when a finger is present.
const uint32_t IR_TARGET_LOW  = 25000;   // if below, nudge brightness up
const uint32_t IR_TARGET_HIGH = 90000;   // if above, nudge brightness down
const byte     LED_MIN        = 20;
const byte     LED_MAX        = 255;
const byte     LED_STEP       = 5;       // gentle changes to avoid oscillation
byte           ledCurrent     = LED_BRIGHTNESS;
uint32_t       lastGainAdjust = 0;
const uint32_t GAIN_PERIOD_MS = 800;     // adjust at most ~1.25 Hz

// ===== Display smoothing (EMA on *displayed* values) =====
int displaySpo2 = -1;
int displayHr   = -1;
// alpha_numer/alpha_denom = smoothing factor; bigger denom = more smoothing.
const int EMA_NUM   = 3;
const int EMA_DENOM = 10;

// ===== Forward decl =====
void renderOLED();

// ===== Sensor =====
void setupSensor() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  // OLED first
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 not found at 0x3C. Check wiring/address.");
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("OLED OK");
    display.display();
  }

  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 not found. Check wiring.");
    if (display.width() > 0) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("MAX30102 not found");
      display.println("Check wiring/I2C");
      display.display();
    }
    while (1) delay(1000);
  }

  sensor.setup(LED_BRIGHTNESS, SAMPLE_AVG, LED_MODE, SAMPLE_RATE, PULSE_WIDTH, ADC_RANGE);
  sensor.setPulseAmplitudeRed(LED_BRIGHTNESS);
  sensor.setPulseAmplitudeIR(LED_BRIGHTNESS);
  sensor.setPulseAmplitudeGreen(0);  // not used
  ledCurrent = LED_BRIGHTNESS;

  Serial.println("MAX30102 initialised.");

  if (display.width() > 0) {
    display.setCursor(0, 16);
    display.println("Sensor init OK");
    display.display();
  }
}

static inline int emaUpdate(int current, int newVal) {
  if (current < 0) return newVal; // first sample
  return ( (EMA_DENOM - EMA_NUM) * current + EMA_NUM * newVal ) / EMA_DENOM;
}

void computeIfReady() {
  if (bufCount < BUFLEN) return;

  // Only compute if we actually have a finger
  if (lastIR < FINGER_IR_THRESHOLD) {
    validSpo2 = 0;
    validHr = 0;
    return;
  }
