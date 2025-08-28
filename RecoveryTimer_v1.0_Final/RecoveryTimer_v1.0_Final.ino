/*
 * ========================================================================================
 * RECOVERY TIMER PULSE OXIMETER - Year 12 Electronics Assessment
 * 
 * This device measures how quickly a person's heart rate and oxygen levels (SpO₂) 
 * return to normal after physical exercise. It's a fitness recovery monitoring tool.
 * 
 * HOW IT WORKS:
 * 1. User places finger on sensor and presses start button
 * 2. Device captures 10-second baseline reading of normal HR and SpO₂
 * 3. User removes finger, does exercise (jumping jacks, running, etc.)
 * 4. User places finger back on sensor - timer automatically starts
 * 5. Device monitors until readings return to target levels (SpO₂ ≥96%, HR within 10% of baseline)
 * 6. Shows recovery time and saves data for analysis
 * 
 * COMPONENTS USED:
 * - ESP32: Main microcontroller with WiFi/Bluetooth capabilities
 * - MAX30102: Optical sensor that measures heart rate and blood oxygen
 * - SSD1306 OLED: 128x32 display for showing readings and status
 * - Push button: Single button interface for all controls
 * - Buzzer: Audio feedback for user guidance
 * - SPIFFS: File system for storing session history
 * ========================================================================================
 */

// Required libraries for hardware communication and algorithms
#include <Wire.h>           // I2C communication protocol
#include <string.h>         // String manipulation functions
#include "MAX30105.h"       // SparkFun library for MAX30102 sensor
#include "spo2_algorithm.h" // Algorithm to calculate SpO₂ from sensor data
#include <SPIFFS.h>         // ESP32 file system for data storage

// ===== OLED DISPLAY CONFIGURATION =====
// Using Adafruit libraries for SSD1306 OLED display
#include <Adafruit_GFX.h>      // Graphics library for drawing text/shapes
#include <Adafruit_SSD1306.h>  // Specific driver for SSD1306 OLED chips
#define OLED_WIDTH 128         // Display width in pixels
#define OLED_HEIGHT 32         // Display height in pixels  
#define OLED_ADDR 0x3C         // I2C address of the OLED (standard for most 128x32 displays)
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);  // Create display object

// ===== I2C PIN CONFIGURATION =====
// ESP32 allows custom I2C pins - using standard pins for consistency
#define I2C_SDA 21  // Serial Data pin for I2C bus (both OLED and MAX30102 use this)
#define I2C_SCL 22  // Serial Clock pin for I2C bus

// ===== SENSOR INTERRUPT PIN =====  
// MAX30102 can signal when new data is ready via interrupt pin
#define MAX_INT_PIN 19  // GPIO pin connected to MAX30102 INT output

// ===== USER INTERFACE HARDWARE =====
#define BTN_MAIN 25     // Main control button (all functions controlled by this single button)
#define BUZZER_PIN 15   // Active buzzer for audio feedback to user

// ===== BUTTON INPUT HANDLING =====
// Different button press types trigger different actions
enum ButtonAction {
  NONE,         // No button press detected
  SHORT_PRESS,  // Quick press - main navigation/control
  LONG_PRESS    // Hold 1.5+ seconds - special functions like data export
};

// ===== SYSTEM STATE MACHINE =====
// The device operates in distinct states to guide the user through the process
enum TimerState {
  IDLE,               // Ready state - waiting for user to start
  BASELINE_CAPTURE,   // Recording resting heart rate and SpO₂ for 10 seconds
  WAIT_FOR_EXERCISE,  // User should exercise, then place finger back on sensor
  TIMING,             // Measuring recovery time until target levels reached
  COMPLETE            // Session finished - showing results
};

// ===== STATE TRACKING VARIABLES =====
TimerState currentState = IDLE;        // Current system state
unsigned long sessionStartTime = 0;    // When recovery timing began (milliseconds)
unsigned long sessionEndTime = 0;      // When recovery timing ended
unsigned long baselineStartTime = 0;   // When baseline capture started
unsigned long exerciseWaitStart = 0;   // When we started waiting for exercise
int baselineHR = 0;                    // Resting heart rate captured during baseline
int baselineSpO2 = 0;                  // Resting oxygen saturation captured during baseline
int minSpO2 = 100;                     // Lowest SpO₂ seen during recovery (starts at max)
int maxHR = 0;                         // Highest heart rate seen during recovery (starts at min)
bool longPressHandled = false;         // Prevents multiple long-press triggers

// ===== RECOVERY TARGET THRESHOLDS =====
// These values determine when the user has "recovered" from exercise
const int TARGET_SPO2 = 96;                    // SpO₂ must reach 96% or higher (normal is 95-100%)
const float HR_RECOVERY_FACTOR = 0.1;          // Heart rate must be within 10% of baseline (e.g., if baseline was 70, target is 63-77)
const unsigned long SESSION_TIMEOUT = 300000;  // Maximum session time: 5 minutes (300,000 ms)
const unsigned long BASELINE_DURATION = 10000; // Baseline capture time: 10 seconds (10,000 ms)
const unsigned long EXERCISE_TIMEOUT = 120000; // Time limit for user to complete exercise: 2 minutes (120,000 ms)

// ===== DATA STORAGE STRUCTURE =====
// Each workout session is stored as a struct with key metrics
struct Session {
  uint32_t timestamp;      // When the session occurred (milliseconds since boot)
  uint16_t duration;       // How long recovery took (seconds)
  uint8_t minSpO2;         // Lowest oxygen saturation during recovery
  uint8_t maxHR;           // Highest heart rate during recovery  
  uint8_t baselineHR;      // Resting heart rate before exercise
  bool targetReached;      // Did user reach recovery targets automatically (true) or stop manually (false)
};

// ===== SESSION STORAGE ARRAYS =====
#define MAX_SESSIONS 20                 // Store up to 20 sessions (oldest deleted when full)
Session sessions[MAX_SESSIONS];         // Array to hold session data in memory
int sessionCount = 0;                   // How many sessions are currently stored

// ===== INTERRUPT HANDLING =====
// MAX30102 sensor uses interrupts to signal when new data is ready
volatile bool dataReady = false;       // Flag set by interrupt routine

// Interrupt Service Routine - runs when MAX30102 INT pin goes LOW
// IRAM_ATTR ensures this code runs from fast RAM, not flash memory
void IRAM_ATTR maxInterrupt() {
  dataReady = true;                     // Signal main loop that sensor has new data
}

// ===== MAX30102 SENSOR CONFIGURATION =====
MAX30105 sensor;                        // Create sensor object using SparkFun library
const byte LED_BRIGHTNESS = 220;        // LED intensity (0-255) - bright enough for good signal, not too bright to heat up
const byte SAMPLE_AVG = 8;               // Average 8 samples per reading (reduces noise)
const byte LED_MODE = 2;                 // Mode 2 = Red + IR LEDs (needed for SpO₂ calculation)
const byte SAMPLE_RATE = 100;            // 100 samples per second (100 Hz sampling rate)
const int PULSE_WIDTH = 411;             // LED pulse width in microseconds (longer = more light, better SNR)
const int ADC_RANGE = 16384;             // Analog-to-digital converter range (higher = more resolution)

// ===== ALGORITHM DATA BUFFERS =====
// SpO₂ algorithm needs a rolling window of samples to detect heart beats and calculate oxygen
const int32_t BUFLEN = 100;             // Buffer size: 100 samples = 1 second at 100 Hz
uint32_t irBuf[BUFLEN];                  // Infrared light readings (used for heart rate detection)
uint32_t redBuf[BUFLEN];                 // Red light readings (used with IR for SpO₂ calculation)
int bufCount = 0;                        // Current number of samples in buffer

// ===== CALCULATED VALUES =====
// These are calculated by the SpO₂ algorithm and updated continuously
volatile int32_t spo2 = -1;              // Blood oxygen saturation percentage (-1 = invalid)
volatile int8_t validSpo2 = 0;           // 1 = SpO₂ reading is valid, 0 = invalid/unreliable
volatile int32_t heartRate = -1;         // Heart rate in beats per minute (-1 = invalid)
volatile int8_t validHr = 0;             // 1 = heart rate reading is valid, 0 = invalid
volatile uint32_t lastIR = 0;            // Most recent IR reading (used to detect finger presence)

// ===== FINGER DETECTION =====
const uint32_t FINGER_IR_THRESHOLD = 15000;  // IR value above this means finger is detected on sensor

// ===== AUTOMATIC GAIN CONTROL =====
// The sensor automatically adjusts LED brightness for different finger sizes/skin tones
const uint32_t IR_TARGET_LOW = 25000;    // If IR reading below this, increase LED brightness
const uint32_t IR_TARGET_HIGH = 90000;   // If IR reading above this, decrease LED brightness  
const byte LED_MIN = 20;                 // Minimum LED brightness (too low = weak signal)
const byte LED_MAX = 255;                // Maximum LED brightness (too high = sensor overload)
const byte LED_STEP = 5;                 // How much to adjust brightness each time
byte ledCurrent = LED_BRIGHTNESS;        // Current LED brightness setting
uint32_t lastGainAdjust = 0;             // Last time we adjusted gain (prevent too frequent changes)
const uint32_t GAIN_PERIOD_MS = 800;     // Wait 800ms between gain adjustments

// ===== DISPLAY VALUE SMOOTHING =====
// Raw sensor readings are noisy - we smooth them using Exponential Moving Average (EMA)
int displaySpo2 = -1;                    // Smoothed SpO₂ value shown on display (-1 = no reading yet)
int displayHr = -1;                      // Smoothed heart rate value shown on display (-1 = no reading yet)
const int EMA_NUM = 3;                   // EMA numerator (how much weight new values get)
const int EMA_DENOM = 10;                // EMA denominator (smoothing factor: new = 3/10, old = 7/10)

// ===== FUNCTION DECLARATIONS =====
// These functions are defined later in the code but used earlier, so we declare them here
ButtonAction getButtonAction();          // Checks if button was pressed and what type of press
void handleTimerStateMachine();          // Main logic for controlling device states

// ===== BUTTON INPUT PROCESSING =====
/*
 * This function detects different types of button presses:
 * - SHORT_PRESS: Quick press and release (main navigation)
 * - LONG_PRESS: Hold button for 1.5+ seconds (special functions)
 * - NONE: No button activity
 * 
 * Uses debouncing to prevent false triggers from electrical noise
 */
ButtonAction getButtonAction() {
  static bool lastState = HIGH;           // Previous button state (HIGH = not pressed, LOW = pressed)
  static unsigned long pressStart = 0;    // When the current press started (for timing long presses)
  
  bool currentState = digitalRead(BTN_MAIN);  // Read current button state
  
  // Button just pressed down (edge detection: HIGH -> LOW)
  if (currentState == LOW && lastState == HIGH) {
    pressStart = millis();                // Record when press started
    longPressHandled = false;            // Reset long press flag
  }
  // Button is held down - check if it's been long enough for long press
  else if (currentState == LOW && !longPressHandled) {
    if (millis() - pressStart > 1500) {  // 1.5 seconds = long press threshold
      longPressHandled = true;           // Prevent multiple long press triggers
      lastState = currentState;
      return LONG_PRESS;
    }
  }
  // Button just released (edge detection: LOW -> HIGH)
  else if (currentState == HIGH && lastState == LOW) {
    if (!longPressHandled && millis() - pressStart > 50) {  // 50ms debounce time
      lastState = currentState;
      return SHORT_PRESS;
    }
  }
  
  lastState = currentState;             // Update state for next call
  return NONE;                          // No button action detected
}

// ===== AUDIO FEEDBACK FUNCTIONS =====
/*
 * These functions provide audio cues to guide the user through the process
 * Different beep patterns indicate different events/states
 */

// Single beep - basic confirmation sound
void beep(int duration) {
  digitalWrite(BUZZER_PIN, HIGH);       // Turn buzzer on
  delay(duration);                      // Wait for specified time (milliseconds)
  digitalWrite(BUZZER_PIN, LOW);        // Turn buzzer off
}

// Two quick beeps - indicates baseline captured, timing phase starting
void doubleBeep() {
  beep(100);                           // First beep: 100ms
  delay(100);                          // Gap: 100ms
  beep(100);                           // Second beep: 100ms
}

// Three beeps - indicates successful automatic recovery (target reached)
void tripleBeep() {
  for(int i = 0; i < 3; i++) {         // Repeat 3 times
    beep(200);                         // Longer beep: 200ms each
    if(i < 2) delay(100);              // 100ms gap between beeps (not after last one)
  }
}

// ===== SESSION DATA MANAGEMENT =====
/*
 * This function saves the current workout session data to memory and persistent storage.
 * Each session contains: timestamp, duration, min SpO₂, max HR, baseline HR, and whether targets were reached
 */
void saveSession(bool targetReached) {
  // Create new session record with current data
  Session newSession;
  newSession.timestamp = millis();                                    // When session occurred (ms since boot)
  newSession.duration = (sessionEndTime - sessionStartTime) / 1000;   // Recovery time in seconds
  newSession.minSpO2 = minSpO2;                                       // Lowest oxygen level during recovery
  newSession.maxHR = maxHR;                                          // Highest heart rate during recovery
  newSession.baselineHR = baselineHR;                                // Resting heart rate before exercise
  newSession.targetReached = targetReached;                          // Auto-stop (true) or manual stop (false)
  
  // Add to session array (circular buffer - oldest data gets overwritten when full)
  if (sessionCount >= MAX_SESSIONS) {
    // Shift all sessions down by one position (remove oldest)
    memmove(&sessions[0], &sessions[1], (MAX_SESSIONS - 1) * sizeof(Session));
    sessions[MAX_SESSIONS - 1] = newSession;                         // Add new session at end
  } else {
    sessions[sessionCount++] = newSession;                           // Add new session, increment counter
  }
  
  // Save to SPIFFS flash memory (data survives power cycles)
  File f = SPIFFS.open("/sessions.dat", FILE_WRITE);
  if (f) {
    f.write((uint8_t*)sessions, sizeof(Session) * sessionCount);     // Write session array
    f.write((uint8_t*)&sessionCount, sizeof(sessionCount));          // Write session count
    f.close();
  }
}

/*
 * This function loads previously saved session data from flash memory when device boots up.
 * This allows session history to persist even when device is powered off.
 */
void loadSessions() {
  if (SPIFFS.exists("/sessions.dat")) {                    // Check if session file exists
    File f = SPIFFS.open("/sessions.dat", FILE_READ);      // Open file for reading
    if (f && f.size() > 0) {                              // Make sure file is valid and not empty
      int dataSize = f.size() - sizeof(sessionCount);     // Calculate size of session data
      if (dataSize > 0) {
        f.read((uint8_t*)sessions, dataSize);              // Load session array from file
        f.read((uint8_t*)&sessionCount, sizeof(sessionCount)); // Load session count
      }
      f.close();
    }
  }
}

/*
 * This function exports all stored session data as CSV format to the Serial Monitor.
 * Users can copy this data into Excel or other spreadsheet programs for analysis.
 * Triggered by long-pressing the button when device is in IDLE state.
 */
void exportCSV() {
  Serial.println("\n=== CSV Export ===");
  // Print CSV header row
  Serial.println("Session,Timestamp,Duration,MinSpO2,MaxHR,BaselineHR,TargetReached");
  
  // Print each session as a CSV row
  for (int i = 0; i < sessionCount; i++) {
    Serial.print(i + 1);                                    // Session number (1, 2, 3...)
    Serial.print(",");
    Serial.print(sessions[i].timestamp);                    // When session occurred (ms)
    Serial.print(",");
    Serial.print(sessions[i].duration);                     // Recovery time (seconds)
    Serial.print(",");
    Serial.print(sessions[i].minSpO2);                      // Lowest oxygen % during recovery
    Serial.print(",");
    Serial.print(sessions[i].maxHR);                        // Highest heart rate during recovery
    Serial.print(",");
    Serial.print(sessions[i].baselineHR);                   // Resting heart rate before exercise
    Serial.print(",");
    Serial.println(sessions[i].targetReached ? "Yes" : "No"); // Did automatic recovery complete?
  }
  Serial.println("=== End Export ===\n");
}

// ===== HARDWARE INITIALIZATION =====
/*
 * This function initializes all the hardware components:
 * - I2C bus for OLED display and MAX30102 sensor
 * - OLED display with startup message
 * - MAX30102 sensor with optimal settings for SpO₂ measurement
 * - Interrupt handling for efficient sensor data collection
 */
void setupSensor() {
  // Initialize I2C bus with custom pins and set to fast mode (400kHz)
  Wire.begin(I2C_SDA, I2C_SCL);          // SDA=21, SCL=22 for ESP32
  Wire.setClock(400000);                  // 400kHz I2C speed for faster data transfer

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 not found");  // Display initialization failed
  } else {
    // Show startup screen
    display.clearDisplay();               // Clear any previous content
    display.setTextColor(SSD1306_WHITE);  // White text on black background
    display.setTextSize(1);               // Small text size
    display.setCursor(0, 0);              // Top-left corner
    display.println("Recovery Timer v1.0"); // Project title
    display.display();                    // Actually show the text
    delay(1000);                          // Display for 1 second
  }

  // Initialize MAX30102 sensor
  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 not found"); // Sensor initialization failed
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Sensor Error!");     // Show error on display
    display.display();
    while (1) delay(1000);                // Stop here - can't continue without sensor
  }

  // Configure sensor with optimal settings for SpO₂ measurement
  sensor.setup(LED_BRIGHTNESS, SAMPLE_AVG, LED_MODE, SAMPLE_RATE, PULSE_WIDTH, ADC_RANGE);
  sensor.setPulseAmplitudeRed(LED_BRIGHTNESS);   // Set red LED brightness
  sensor.setPulseAmplitudeIR(LED_BRIGHTNESS);    // Set infrared LED brightness
  sensor.setPulseAmplitudeGreen(0);              // Turn off green LED (not needed for SpO₂)
  ledCurrent = LED_BRIGHTNESS;                   // Initialize auto-gain variable

  // Setup interrupt for efficient data collection
  pinMode(MAX_INT_PIN, INPUT_PULLUP);            // INT pin with pull-up resistor
  attachInterrupt(digitalPinToInterrupt(MAX_INT_PIN), maxInterrupt, FALLING); // Trigger on falling edge
}

/*
 * Exponential Moving Average filter to smooth noisy sensor readings
 * Formula: new_average = (old_average * 0.7) + (new_reading * 0.3)
 * This reduces noise while still responding to real changes
 */
static inline int emaUpdate(int current, int newVal) {
  if (current < 0) return newVal;        // First reading - no previous average
  return ((EMA_DENOM - EMA_NUM) * current + EMA_NUM * newVal) / EMA_DENOM;
}

/*
 * This function calculates SpO₂ and heart rate from the collected sensor data.
 * It uses Maxim's algorithm which analyzes the ratio of red to infrared light
 * absorption to determine blood oxygen levels and detect heart beats.
 */
void computeIfReady() {
  if (bufCount < BUFLEN) return;          // Need full buffer (100 samples) for calculation
  
  // Check if finger is actually on sensor
  if (lastIR < FINGER_IR_THRESHOLD) {
    validSpo2 = 0;                        // Mark readings as invalid
    validHr = 0;
    return;
  }

  // Run Maxim's SpO₂ algorithm on the collected data
  int32_t s, hr;                          // Variables to receive calculated values
  int8_t vs, vhr;                         // Variables to receive validity flags
  maxim_heart_rate_and_oxygen_saturation(irBuf, BUFLEN, redBuf, &s, &vs, &hr, &vhr);

  // Store raw algorithm results
  spo2 = s;
  validSpo2 = vs;
  heartRate = hr;
  validHr = vhr;

  // Apply smoothing filter to valid readings for display
  if (validSpo2 && spo2 > 0 && spo2 <= 100) {      // SpO₂ range check (0-100%)
    displaySpo2 = emaUpdate(displaySpo2, (int)spo2);
  }
  if (validHr && heartRate > 0 && heartRate < 240) { // Heart rate range check (0-240 BPM)
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

// ===== MAIN PROGRAM ENTRY POINT =====
/*
 * setup() runs once when the ESP32 boots up
 * This function initializes all systems and prepares for operation
 */
void setup() {
  // Initialize serial communication for debugging and CSV export
  Serial.begin(115200);                   // 115200 baud rate (fast serial communication)
  delay(200);                             // Give serial time to initialize
  
  // Initialize SPIFFS file system for persistent data storage
  if (!SPIFFS.begin(true)) {              // true = format if mount fails
    Serial.println("SPIFFS Mount Failed");
  } else {
    loadSessions();                       // Load previously saved session data
    Serial.print("Loaded ");
    Serial.print(sessionCount);
    Serial.println(" sessions");
  }
  
  // Configure GPIO pins
  pinMode(BTN_MAIN, INPUT_PULLUP);        // Button with internal pull-up resistor
  pinMode(BUZZER_PIN, OUTPUT);            // Buzzer as output pin
  digitalWrite(BUZZER_PIN, LOW);          // Make sure buzzer starts off
  
  // Initialize I2C devices (OLED display and MAX30102 sensor)
  setupSensor();
  
  // System ready - provide audio and text confirmation
  beep(100);                              // Single beep = system ready
  Serial.println("Recovery Timer Ready");
  Serial.println("Short press: Start/Stop");
  Serial.println("Long press (idle): Export CSV");
}

/*
 * loop() runs continuously after setup() completes
 * This is the main program loop that handles all ongoing operations
 */
void loop() {
  // Process button presses and manage device states (IDLE, BASELINE, TIMING, etc.)
  handleTimerStateMachine();
  
  // Collect data from MAX30102 sensor
  if (dataReady) {                      // Interrupt flag set - sensor has new data
    dataReady = false;                  // Clear flag
    processSensorData();                // Read and buffer the new samples
  }
  else if (sensor.available()) {        // Check if sensor has data (polling method)
    processSensorData();
  } 
  else {
    sensor.check();                     // Keep sensor communication alive
  }

  // Update calculations and display every 1 second (1 Hz)
  static uint32_t lastCalc = 0;         // Track when we last updated display
  if (millis() - lastCalc >= 1000) {    // 1000ms = 1 second interval
    computeIfReady();                   // Calculate SpO₂ and heart rate from buffered data
    renderOLED();                       // Update OLED display with current information
    lastCalc = millis();                // Reset timer for next update
  }

  // Continuously adjust sensor LED brightness for optimal readings
  maybeAdjustAutoGain();
  
  // Small delay to prevent overwhelming the system
  delay(2);                             // 2ms delay allows other processes to run
}