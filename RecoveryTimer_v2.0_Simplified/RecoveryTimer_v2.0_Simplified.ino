/*
 * ========================================================================================
 * RECOVERY TIMER PULSE OXIMETER v2.0 - SIMPLIFIED VERSION
 * Year 12 Electronics Assessment - Clean Architecture with Wrapper Library
 * 
 * This simplified version uses the PulseOximeterWrapper library to handle all the
 * complex sensor operations, making the main code much easier to understand and maintain.
 * 
 * The wrapper library handles:
 * - Sensor initialization and configuration
 * - Raw data collection and buffering  
 * - SpO₂ and heart rate calculations
 * - Exponential Moving Average smoothing
 * - Automatic gain control
 * - Finger detection
 * 
 * This allows the main program to focus purely on the recovery timer logic.
 * ========================================================================================
 */

#include <Wire.h>
#include <SPIFFS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "PulseOximeterWrapper.h"

// ===== HARDWARE CONFIGURATION =====
#define I2C_SDA 21
#define I2C_SCL 22
#define BTN_MAIN 25
#define BUZZER_PIN 15

// OLED Display
#define OLED_WIDTH 128
#define OLED_HEIGHT 32
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// Pulse Oximeter (using our wrapper library)
PulseOximeterWrapper oximeter;

// ===== SYSTEM STATES =====
enum TimerState {
  IDLE,               // Ready - waiting for user to start
  BASELINE_CAPTURE,   // Recording resting vitals for 10 seconds
  WAIT_FOR_EXERCISE,  // User should exercise, then return finger to sensor
  TIMING,             // Measuring recovery time until targets reached
  COMPLETE            // Session finished - showing results
};

TimerState currentState = IDLE;

// ===== BUTTON HANDLING =====
enum ButtonAction { NONE, SHORT_PRESS, LONG_PRESS };

bool lastButtonState = HIGH;
unsigned long pressStart = 0;
bool longPressHandled = false;

// ===== RECOVERY TARGETS =====
const int TARGET_SPO2 = 96;              // SpO₂ must reach 96% or higher
const float HR_RECOVERY_FACTOR = 0.1;    // Heart rate within 10% of baseline
const unsigned long SESSION_TIMEOUT = 300000;      // 5 minutes max
const unsigned long BASELINE_DURATION = 10000;     // 10 seconds baseline
const unsigned long EXERCISE_TIMEOUT = 120000;     // 2 minutes for exercise

// ===== SESSION TRACKING =====
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

// ===== STATE VARIABLES =====
unsigned long sessionStartTime = 0;
unsigned long sessionEndTime = 0;
unsigned long baselineStartTime = 0;
unsigned long exerciseWaitStart = 0;
int baselineHR = 0;
int baselineSpO2 = 0;
int minSpO2 = 100;
int maxHR = 0;

// ===== FUNCTION DECLARATIONS =====
ButtonAction getButtonAction();
void handleStateMachine();
void renderDisplay();
void beep(int duration);
void saveSession(bool targetReached);
void loadSessions();
void exportCSV();

void setup() {
  Serial.begin(115200);
  Serial.println("Recovery Timer v2.0 - Simplified");
  
  // Initialize I2C bus
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  
  // Initialize SPIFFS for data storage
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
  } else {
    loadSessions();
    Serial.print("Loaded ");
    Serial.print(sessionCount);
    Serial.println(" sessions");
  }
  
  // Initialize GPIO pins
  pinMode(BTN_MAIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED not found");
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Recovery Timer v2.0");
    display.println("Simplified Version");
    display.display();
    delay(2000);
  }
  
  // Initialize pulse oximeter wrapper
  if (!oximeter.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 sensor not found!");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Sensor Error!");
    display.display();
    while (1) delay(1000);
  }
  
  Serial.println("System ready!");
  beep(100);
}

void loop() {
  // Update sensor readings (wrapper handles all complexity)
  oximeter.update();
  
  // Handle user interface and state transitions
  handleStateMachine();
  
  // Update display every 500ms
  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate >= 500) {
    renderDisplay();
    lastDisplayUpdate = millis();
  }
  
  delay(10);
}

// ===== BUTTON PROCESSING =====
ButtonAction getButtonAction() {
  bool currentState = digitalRead(BTN_MAIN);
  
  if (currentState == LOW && lastButtonState == HIGH) {
    pressStart = millis();
    longPressHandled = false;
  }
  else if (currentState == LOW && !longPressHandled) {
    if (millis() - pressStart > 1500) {
      longPressHandled = true;
      lastButtonState = currentState;
      return LONG_PRESS;
    }
  }
  else if (currentState == HIGH && lastButtonState == LOW) {
    if (!longPressHandled && millis() - pressStart > 50) {
      lastButtonState = currentState;
      return SHORT_PRESS;
    }
  }
  
  lastButtonState = currentState;
  return NONE;
}

// ===== STATE MACHINE =====
void handleStateMachine() {
  ButtonAction action = getButtonAction();
  
  switch(currentState) {
    case IDLE:
      if (action == SHORT_PRESS) {
        if (oximeter.isFingerDetected()) {
          currentState = BASELINE_CAPTURE;
          baselineStartTime = millis();
          beep(100);
          Serial.println("Starting baseline capture...");
        } else {
          beep(50);
          Serial.println("Place finger on sensor first");
        }
      } else if (action == LONG_PRESS) {
        exportCSV();
        beep(200); beep(200); // Double beep for export
      }
      break;
      
    case BASELINE_CAPTURE:
      if (action == SHORT_PRESS) {
        currentState = IDLE;
        beep(50);
        Serial.println("Cancelled");
      }
      else if (millis() - baselineStartTime >= BASELINE_DURATION) {
        if (oximeter.isHeartRateValid() && oximeter.isSpO2Valid()) {
          baselineHR = oximeter.getHeartRate();
          baselineSpO2 = oximeter.getSpO2();
          currentState = WAIT_FOR_EXERCISE;
          exerciseWaitStart = millis();
          minSpO2 = 100;
          maxHR = 0;
          beep(100); delay(100); beep(100); // Double beep
          Serial.print("Baseline: HR=");
          Serial.print(baselineHR);
          Serial.print(" SpO2=");
          Serial.println(baselineSpO2);
        } else {
          currentState = IDLE;
          beep(500); // Error beep
          Serial.println("Invalid baseline - try again");
        }
      }
      break;
      
    case WAIT_FOR_EXERCISE:
      if (oximeter.isFingerDetected() && oximeter.isHeartRateValid()) {
        // Wait 3 seconds for stable reading
        static unsigned long stableStart = 0;
        if (stableStart == 0) stableStart = millis();
        
        if (millis() - stableStart > 3000) {
          int currentHR = oximeter.getHeartRate();
          // Check if HR is elevated (indicating exercise was done)
          if (currentHR > baselineHR * 1.05 || action == SHORT_PRESS) {
            currentState = TIMING;
            sessionStartTime = millis();
            beep(100);
            Serial.println("Recovery timing started!");
            stableStart = 0;
          }
        }
      } else {
        static unsigned long stableStart = 0;
        stableStart = 0; // Reset if finger removed
      }
      
      // Cancel or timeout
      if (action == LONG_PRESS) {
        currentState = IDLE;
        beep(50);
      } else if (millis() - exerciseWaitStart > EXERCISE_TIMEOUT) {
        currentState = IDLE;
        beep(1000);
        Serial.println("Exercise timeout");
      }
      break;
      
    case TIMING: {
      // Track min/max values during recovery
      if (oximeter.isSpO2Valid()) {
        int spo2 = oximeter.getSpO2();
        if (spo2 < minSpO2) minSpO2 = spo2;
      }
      if (oximeter.isHeartRateValid()) {
        int hr = oximeter.getHeartRate();
        if (hr > maxHR) maxHR = hr;
      }
      
      // Check if recovery targets are reached
      bool targetReached = false;
      if (oximeter.isSpO2Valid() && oximeter.isHeartRateValid()) {
        int currentSpO2 = oximeter.getSpO2();
        int currentHR = oximeter.getHeartRate();
        bool spo2Good = (currentSpO2 >= TARGET_SPO2);
        bool hrGood = (abs(currentHR - baselineHR) <= baselineHR * HR_RECOVERY_FACTOR);
        targetReached = spo2Good && hrGood;
      }
      
      // End session
      if (action == SHORT_PRESS || targetReached) {
        sessionEndTime = millis();
        currentState = COMPLETE;
        saveSession(targetReached);
        
        if (targetReached) {
          beep(200); delay(100); beep(200); delay(100); beep(200); // Triple beep
          Serial.println("Recovery target reached!");
        } else {
          beep(200);
          Serial.println("Manual stop");
        }
      } else if (millis() - sessionStartTime > SESSION_TIMEOUT) {
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

// ===== DISPLAY RENDERING =====
void renderDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  
  // First line: State-specific information
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
    
    case WAIT_FOR_EXERCISE:
      if (oximeter.isFingerDetected()) {
        display.println("READY? [Press/Wait]");
      } else {
        display.println("DO EXERCISE NOW!");
      }
      break;
      
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
  
  // Second line: Current readings (if not in exercise wait)
  if (currentState != WAIT_FOR_EXERCISE || oximeter.isFingerDetected()) {
    display.print("SpO2:");
    if (oximeter.isSpO2Valid()) {
      display.print(oximeter.getSpO2());
      display.print("%");
    } else {
      display.print("--");
    }
    
    display.print(" HR:");
    if (oximeter.isHeartRateValid()) {
      display.print(oximeter.getHeartRate());
      display.print("bpm");
    } else {
      display.print("---");
    }
  }
  
  // Third line: Status
  display.setCursor(0, 24);
  if (!oximeter.isFingerDetected() && currentState != WAIT_FOR_EXERCISE) {
    display.print("No finger");
  } else if (currentState == TIMING && oximeter.isSpO2Valid() && oximeter.isHeartRateValid()) {
    display.print("Target: ");
    if (oximeter.getSpO2() >= TARGET_SPO2) display.print("S");
    if (abs(oximeter.getHeartRate() - baselineHR) <= baselineHR * HR_RECOVERY_FACTOR) display.print("H");
  }
  
  display.display();
}

// ===== AUDIO FEEDBACK =====
void beep(int duration) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
}

// ===== DATA MANAGEMENT =====
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