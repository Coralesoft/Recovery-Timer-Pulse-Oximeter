#include <Wire.h>
#include <string.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <SPIFFS.h>

// ===== OLED (SSD1306) =====
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define OLED_WIDTH 128
#define OLED_HEIGHT 32
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// ===== I2C pins =====
#define I2C_SDA 21
#define I2C_SCL 22

// ===== MAX30102 Interrupt pin =====
#define MAX_INT_PIN 19

// ===== Single Button & Buzzer =====
#define BTN_MAIN 25
#define BUZZER_PIN 15

// ===== Button handling =====
enum ButtonAction {
  NONE,
  SHORT_PRESS,
  LONG_PRESS
};

// ===== Timer State Machine =====
enum TimerState {
  IDLE,
  BASELINE_CAPTURE,
  WAIT_FOR_EXERCISE,  // New state - waiting for user to exercise
  TIMING,
  COMPLETE
};

TimerState currentState = IDLE;
unsigned long sessionStartTime = 0;
unsigned long sessionEndTime = 0;
unsigned long baselineStartTime = 0;
unsigned long exerciseWaitStart = 0;
int baselineHR = 0;
int baselineSpO2 = 0;
int minSpO2 = 100;
int maxHR = 0;
bool longPressHandled = false;

// ===== Target thresholds =====
const int TARGET_SPO2 = 96;
const float HR_RECOVERY_FACTOR = 0.1;
const unsigned long SESSION_TIMEOUT = 300000; // 5 minutes
const unsigned long BASELINE_DURATION = 10000; // 10 seconds
const unsigned long EXERCISE_TIMEOUT = 120000; // 2 minutes to start exercise

// ===== Session Storage =====
struct Session {
  uint32_t timestamp;
  uint16_t duration;
  uint8_t minSpO2;
  uint8_t maxHR;
  uint8_t baselineHR;
  bool targetReached;
};

#define MAX_SESSIONS 20
Session sessions[MAX_SESSIONS];
int sessionCount = 0;

// ===== Interrupt handling =====
volatile bool dataReady = false;

void IRAM_ATTR maxInterrupt() {
  dataReady = true;
}

// ===== MAX30102 config =====
MAX30105 sensor;
const byte LED_BRIGHTNESS = 220;
const byte SAMPLE_AVG = 8;
const byte LED_MODE = 2;
const byte SAMPLE_RATE = 100;
const int PULSE_WIDTH = 411;
const int ADC_RANGE = 16384;

// ===== Algorithm buffers =====
const int32_t BUFLEN = 100;
uint32_t irBuf[BUFLEN];
uint32_t redBuf[BUFLEN];
int bufCount = 0;

volatile int32_t spo2 = -1;
volatile int8_t validSpo2 = 0;
volatile int32_t heartRate = -1;
volatile int8_t validHr = 0;
volatile uint32_t lastIR = 0;

const uint32_t FINGER_IR_THRESHOLD = 15000;

// ===== Auto-gain settings =====
const uint32_t IR_TARGET_LOW = 25000;
const uint32_t IR_TARGET_HIGH = 90000;
const byte LED_MIN = 20;
const byte LED_MAX = 255;
const byte LED_STEP = 5;
byte ledCurrent = LED_BRIGHTNESS;
uint32_t lastGainAdjust = 0;
const uint32_t GAIN_PERIOD_MS = 800;

// ===== Display smoothing =====
int displaySpo2 = -1;
int displayHr = -1;
const int EMA_NUM = 3;
const int EMA_DENOM = 10;

// ===== Function declarations =====
ButtonAction getButtonAction();
void handleTimerStateMachine();

// ===== Button handling implementation =====
ButtonAction getButtonAction() {
  static bool lastState = HIGH;
  static unsigned long pressStart = 0;
  
  bool currentState = digitalRead(BTN_MAIN);
  
  if (currentState == LOW && lastState == HIGH) {
    pressStart = millis();
    longPressHandled = false;
  }
  else if (currentState == LOW && !longPressHandled) {
    if (millis() - pressStart > 1500) {
      longPressHandled = true;
      lastState = currentState;
      return LONG_PRESS;
    }
  }
  else if (currentState == HIGH && lastState == LOW) {
    if (!longPressHandled && millis() - pressStart > 50) {
      lastState = currentState;
      return SHORT_PRESS;
    }
  }
  
  lastState = currentState;
  return NONE;
}

// ===== Buzzer functions =====
void beep(int duration) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
}

void doubleBeep() {
  beep(100);
  delay(100);
  beep(100);
}

void tripleBeep() {
  for(int i = 0; i < 3; i++) {
    beep(200);
    if(i < 2) delay(100);
  }
}

// ===== Session Management =====
void saveSession(bool targetReached) {
  Session newSession;
  newSession.timestamp = millis();
  newSession.duration = (sessionEndTime - sessionStartTime) / 1000;
  newSession.minSpO2 = minSpO2;
  newSession.maxHR = maxHR;
  newSession.baselineHR = baselineHR;
  newSession.targetReached = targetReached;
  
  if (sessionCount >= MAX_SESSIONS) {
    memmove(&sessions[0], &sessions[1], (MAX_SESSIONS - 1) * sizeof(Session));
    sessions[MAX_SESSIONS - 1] = newSession;
  } else {
    sessions[sessionCount++] = newSession;
  }
  
  // Save to SPIFFS
  File f = SPIFFS.open("/sessions.dat", FILE_WRITE);
  if (f) {
    f.write((uint8_t*)sessions, sizeof(Session) * sessionCount);
    f.write((uint8_t*)&sessionCount, sizeof(sessionCount));
    f.close();
  }
}

void loadSessions() {
  if (SPIFFS.exists("/sessions.dat")) {
    File f = SPIFFS.open("/sessions.dat", FILE_READ);
    if (f && f.size() > 0) {
      int dataSize = f.size() - sizeof(sessionCount);
      if (dataSize > 0) {
        f.read((uint8_t*)sessions, dataSize);
        f.read((uint8_t*)&sessionCount, sizeof(sessionCount));
      }
      f.close();
    }
  }
}

void exportCSV() {
  Serial.println("\n=== CSV Export ===");
  Serial.println("Session,Timestamp,Duration,MinSpO2,MaxHR,BaselineHR,TargetReached");
  for (int i = 0; i < sessionCount; i++) {
    Serial.print(i + 1);
    Serial.print(",");
    Serial.print(sessions[i].timestamp);
    Serial.print(",");
    Serial.print(sessions[i].duration);
    Serial.print(",");
    Serial.print(sessions[i].minSpO2);
    Serial.print(",");
    Serial.print(sessions[i].maxHR);
    Serial.print(",");
    Serial.print(sessions[i].baselineHR);
    Serial.print(",");
    Serial.println(sessions[i].targetReached ? "Yes" : "No");
  }
  Serial.println("=== End Export ===\n");
}

// ===== Sensor functions =====
void setupSensor() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 not found");
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Recovery Timer v1.0");
    display.display();
    delay(1000);
  }

  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 not found");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Sensor Error!");
    display.display();
    while (1) delay(1000);
  }

  sensor.setup(LED_BRIGHTNESS, SAMPLE_AVG, LED_MODE, SAMPLE_RATE, PULSE_WIDTH, ADC_RANGE);
  sensor.setPulseAmplitudeRed(LED_BRIGHTNESS);
  sensor.setPulseAmplitudeIR(LED_BRIGHTNESS);
  sensor.setPulseAmplitudeGreen(0);
  ledCurrent = LED_BRIGHTNESS;

  pinMode(MAX_INT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(MAX_INT_PIN), maxInterrupt, FALLING);
}

static inline int emaUpdate(int current, int newVal) {
  if (current < 0) return newVal;
  return ((EMA_DENOM - EMA_NUM) * current + EMA_NUM * newVal) / EMA_DENOM;
}

void computeIfReady() {
  if (bufCount < BUFLEN) return;
  if (lastIR < FINGER_IR_THRESHOLD) {
    validSpo2 = 0;
    validHr = 0;
    return;
  }

  int32_t s, hr;
  int8_t vs, vhr;
  maxim_heart_rate_and_oxygen_saturation(irBuf, BUFLEN, redBuf, &s, &vs, &hr, &vhr);

  spo2 = s;
  validSpo2 = vs;
  heartRate = hr;
  validHr = vhr;

  if (validSpo2 && spo2 > 0 && spo2 <= 100) {
    displaySpo2 = emaUpdate(displaySpo2, (int)spo2);
  }
  if (validHr && heartRate > 0 && heartRate < 240) {
    displayHr = emaUpdate(displayHr, (int)heartRate);
  }
}

void processSensorData() {
  while (sensor.available()) {
    uint32_t red = sensor.getRed();
    uint32_t ir = sensor.getIR();
    sensor.nextSample();
    lastIR = ir;

    if (bufCount < BUFLEN) {
      redBuf[bufCount] = red;
      irBuf[bufCount] = ir;
      bufCount++;
    } else {
      memmove(redBuf, redBuf + 1, (BUFLEN - 1) * sizeof(uint32_t));
      memmove(irBuf, irBuf + 1, (BUFLEN - 1) * sizeof(uint32_t));
      redBuf[BUFLEN - 1] = red;
      irBuf[BUFLEN - 1] = ir;
    }
  }
}

void maybeAdjustAutoGain() {
  uint32_t now = millis();
  if (now - lastGainAdjust < GAIN_PERIOD_MS) return;
  if (lastIR < FINGER_IR_THRESHOLD) return;

  bool changed = false;
  if (lastIR < IR_TARGET_LOW && ledCurrent < LED_MAX) {
    byte next = ledCurrent + LED_STEP;
    ledCurrent = (next > LED_MAX) ? LED_MAX : next;
    changed = true;
  } else if (lastIR > IR_TARGET_HIGH && ledCurrent > LED_MIN) {
    int temp = ledCurrent - LED_STEP;
    ledCurrent = (temp < LED_MIN) ? LED_MIN : (byte)temp;
    changed = true;
  }

  if (changed) {
    sensor.setPulseAmplitudeRed(ledCurrent);
    sensor.setPulseAmplitudeIR(ledCurrent);
    lastGainAdjust = now;
  }
}

// ===== Display rendering =====
void renderOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  // State-specific first line
  switch(currentState) {
    case IDLE:
      display.println("READY [Press Start]");
      break;
      
    case BASELINE_CAPTURE: {
      int remaining = (BASELINE_DURATION - (millis() - baselineStartTime)) / 1000;
      display.print("BASELINE: ");
      display.print(remaining);
      display.println("s");
      break;
    }
    
    case WAIT_FOR_EXERCISE: {
      if (lastIR >= FINGER_IR_THRESHOLD) {
        // Finger detected, show status
        display.println("CHECKING...");
        display.setCursor(0, 8);
        display.print("HR: ");
        if (validHr && displayHr > 0) {
          display.print(displayHr);
          if (displayHr > baselineHR * 1.05) {
            display.print(" READY!");
          } else {
            display.print(" [Press]");
          }
        }
      } else {
        // No finger, show exercise prompt
        display.println("DO EXERCISE NOW!");
        display.setCursor(0, 8);
        display.println("Place finger after");
      }
      break;
    }
    
    case TIMING: {
      int elapsed = (millis() - sessionStartTime) / 1000;
      display.print("RECOVERY: ");
      display.print(elapsed);
      display.println("s");
      break;
    }
    
    case COMPLETE: {
      display.print("DONE! ");
      if (sessionCount > 0) {
        display.print(sessions[sessionCount-1].duration);
        display.println("s");
      }
      break;
    }
  }

  // Current readings (not shown during exercise wait)
  if (currentState != WAIT_FOR_EXERCISE) {
    display.print("SpO2: ");
    if (validSpo2 && displaySpo2 > 0) {
      display.print(displaySpo2);
      display.print("%");
      if (currentState == TIMING && displaySpo2 < minSpO2) {
        minSpO2 = displaySpo2;
      }
    } else {
      display.print("--");
    }

    display.print(" HR: ");
    if (validHr && displayHr > 0) {
      display.print(displayHr);
      display.print("bpm");
      if (currentState == TIMING && displayHr > maxHR) {
        maxHR = displayHr;
      }
    } else {
      display.print("---");
    }

    // Status line
    display.setCursor(0, 24);
    if (lastIR < FINGER_IR_THRESHOLD && currentState != WAIT_FOR_EXERCISE) {
      display.print("No finger");
    } else if (currentState == TIMING) {
      display.print("Target: ");
      if (displaySpo2 >= TARGET_SPO2) display.print("S");
      if (abs(displayHr - baselineHR) <= baselineHR * HR_RECOVERY_FACTOR) display.print("H");
    }
  }

  display.display();
}

// ===== State Machine =====
void handleTimerStateMachine() {
  ButtonAction action = getButtonAction();
  
  switch(currentState) {
    case IDLE:
      if (action == SHORT_PRESS) {
        if (lastIR >= FINGER_IR_THRESHOLD) {
          currentState = BASELINE_CAPTURE;
          baselineStartTime = millis();
          beep(100);
          Serial.println("Baseline capture started");
        } else {
          beep(50);
          Serial.println("Place finger on sensor first");
        }
      } else if (action == LONG_PRESS) {
        exportCSV();
        doubleBeep();
      }
      break;
      
    case BASELINE_CAPTURE:
      if (action == SHORT_PRESS) {
        currentState = IDLE;
        beep(50);
        Serial.println("Cancelled");
      }
      else if (millis() - baselineStartTime >= BASELINE_DURATION) {
        if (validHr && displayHr > 0 && displaySpo2 > 0) {
          baselineHR = displayHr;
          baselineSpO2 = displaySpo2;
          currentState = WAIT_FOR_EXERCISE;
          exerciseWaitStart = millis();
          minSpO2 = 100;
          maxHR = 0;
          doubleBeep();
          Serial.print("Baseline captured! HR: ");
          Serial.print(baselineHR);
          Serial.print(" SpO2: ");
          Serial.println(baselineSpO2);
          Serial.println("Remove finger and do exercise!");
        } else {
          currentState = IDLE;
          beep(500);
          Serial.println("Invalid baseline");
        }
      }
      break;
      
    case WAIT_FOR_EXERCISE:
      // Check if finger is placed back on sensor
      if (lastIR >= FINGER_IR_THRESHOLD) {
        // Wait a moment for readings to stabilize
        static unsigned long fingerPlacedTime = 0;
        if (fingerPlacedTime == 0) {
          fingerPlacedTime = millis();
        }
        
        // After 3 seconds of stable finger placement, check HR
        if (millis() - fingerPlacedTime > 3000 && validHr) {
          // Check if HR is elevated (indicates exercise was done)
          if (displayHr > baselineHR * 1.05) {  // Lowered to 5% increase
            currentState = TIMING;
            sessionStartTime = millis();
            beep(100);
            fingerPlacedTime = 0; // Reset for next time
            Serial.print("Recovery started! HR: ");
            Serial.print(displayHr);
            Serial.print(" (baseline was ");
            Serial.print(baselineHR);
            Serial.println(")");
          } else {
            // Allow manual start even if HR not elevated enough
            if (action == SHORT_PRESS) {
              currentState = TIMING;
              sessionStartTime = millis();
              beep(100);
              fingerPlacedTime = 0;
              Serial.println("Manual recovery start (HR not elevated)");
            }
          }
        }
      } else {
        // Finger removed, reset timer
        static unsigned long fingerPlacedTime = 0;
        fingerPlacedTime = 0;
      }
      
      // Cancel with long press
      if (action == LONG_PRESS) {
        currentState = IDLE;
        beep(50);
        Serial.println("Cancelled");
      }
      
      // Timeout if no activity
      if (millis() - exerciseWaitStart > EXERCISE_TIMEOUT) {
        currentState = IDLE;
        beep(1000);
        Serial.println("Exercise timeout");
      }
      break;
      
    case TIMING: {
      bool targetReached = false;
      
      if (validSpo2 && validHr) {
        bool spo2Recovered = (displaySpo2 >= TARGET_SPO2);
        bool hrRecovered = (abs(displayHr - baselineHR) <= baselineHR * HR_RECOVERY_FACTOR);
        targetReached = spo2Recovered && hrRecovered;
      }
      
      if (action == SHORT_PRESS || targetReached) {
        sessionEndTime = millis();
        currentState = COMPLETE;
        saveSession(targetReached);
        
        if (targetReached) {
          tripleBeep();
          Serial.println("Target reached!");
        } else {
          beep(200);
          Serial.println("Manual stop");
        }
        
        Serial.print("Duration: ");
        Serial.print((sessionEndTime - sessionStartTime) / 1000);
        Serial.print("s, Min SpO2: ");
        Serial.print(minSpO2);
        Serial.print("%, Max HR: ");
        Serial.println(maxHR);
      }
      else if (millis() - sessionStartTime > SESSION_TIMEOUT) {
        sessionEndTime = millis();
        currentState = COMPLETE;
        saveSession(false);
        beep(1000);
        Serial.println("Session timeout");
      }
      break;
    }
      
    case COMPLETE:
      if (action == SHORT_PRESS) {
        currentState = IDLE;
        beep(100);
      }
      break;
  }
}

// ===== Arduino lifecycle =====
void setup() {
  Serial.begin(115200);
  delay(200);
  
  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
  } else {
    loadSessions();
    Serial.print("Loaded ");
    Serial.print(sessionCount);
    Serial.println(" sessions");
  }
  
  // Setup hardware
  pinMode(BTN_MAIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  setupSensor();
  
  beep(100);
  Serial.println("Recovery Timer Ready");
  Serial.println("Short press: Start/Stop");
  Serial.println("Long press (idle): Export CSV");
}

void loop() {
  // Handle state machine
  handleTimerStateMachine();
  
  // Handle sensor data
  if (dataReady) {
    dataReady = false;
    processSensorData();
  }
  else if (sensor.available()) {
    processSensorData();
  } 
  else {
    sensor.check();
  }

  // Update display and compute at 1Hz
  static uint32_t lastCalc = 0;
  if (millis() - lastCalc >= 1000) {
    computeIfReady();
    renderOLED();
    lastCalc = millis();
  }

  maybeAdjustAutoGain();
  delay(2);
}