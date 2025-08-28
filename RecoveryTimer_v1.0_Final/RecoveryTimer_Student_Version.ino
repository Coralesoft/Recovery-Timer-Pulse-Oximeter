/*
 * Recovery Timer Project - Year 12 Electronics
 * Made by: [Student Name]
 * Wellington College 2025
 * 
 * This measures how fast your heart rate and oxygen go back to normal after exercise
 * Uses ESP32 with MAX30102 sensor and OLED display
 * 
 * TODO: Make the readings more accurate
 * TODO: Maybe add sound when target is reached
 */

#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Hardware pins - I used these ones because they work
#define SDA_PIN 21
#define SCL_PIN 22
#define BUTTON_PIN 25
#define buzzer_pin 15  // buzzer pin

// Display stuff
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Sensor
MAX30105 particleSensor;

// States - I could use enum but this is easier
bool is_idle = true;
bool capturing_baseline = false;
bool waiting_for_exercise = false;
bool timing_recovery = false;
bool session_complete = false;

// Button stuff
bool button_pressed = false;
bool last_button_state = HIGH;
unsigned long button_press_time = 0;
bool long_press_done = false;

// Session data - just keep last 15 sessions in memory
struct session_data {
  unsigned long timestamp;
  int duration_seconds;
  int min_spo2;
  int max_heart_rate;
  int baseline_hr;
  bool reached_target;
};
session_data sessions[15]; // Keep last 15 sessions
int session_count = 0;

// Timing variables
unsigned long baseline_start = 0;
unsigned long exercise_wait_start = 0;
unsigned long recovery_start = 0;
unsigned long session_end = 0;

// Baseline values
int baseline_heart_rate = 0;
int baseline_spo2_value = 0;

// Current values
int current_spo2 = -1;
int current_heart_rate = -1;
int smoothed_spo2 = -1;
int smoothed_hr = -1;

// Session tracking
int min_spo2_in_session = 100;
int max_hr_in_session = 0;

// Sensor data buffers
#define BUFFER_SIZE 100
uint32_t ir_buffer[BUFFER_SIZE];
uint32_t red_buffer[BUFFER_SIZE];
int buffer_position = 0;
bool buffer_full = false;

// Algorithm results
int32_t spo2_result;
int8_t spo2_valid;
int32_t hr_result;  
int8_t hr_valid;
uint32_t last_ir_reading = 0;

// Settings - I tried different values, these work best
int finger_threshold = 7000;  // I tried 5000 but 7000 works better
int target_spo2 = 96;  // Normal oxygen level
float hr_recovery_percent = 0.10;  // 10% of baseline
unsigned long baseline_time = 10000;  // 10 seconds
unsigned long max_session_time = 300000;  // 5 minutes max
unsigned long exercise_timeout = 120000;  // 2 minutes to start exercise

// Display timing
unsigned long last_display_update = 0;

// Some test variables I used during development
// int test_counter = 0;  // not using this anymore
// bool debug_mode = false;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Recovery Timer...");
  
  // Setup pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(buzzer_pin, OUTPUT);
  digitalWrite(buzzer_pin, LOW);  // Turn off buzzer
  
  // Start I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);  // Fast I2C
  
  // Initialize display
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("Display failed!");
    while(1);  // Stop if display doesn't work
  }
  
  // Show startup message
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Recovery Timer v1.0");
  display.println("By: [Student Name]");
  display.display();
  delay(2000);
  
  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 not found!");
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Sensor Error!");
    display.display();
    while(1);  // Stop if sensor doesn't work
  }
  
  Serial.println("Sensor found!");
  
  // Setup sensor - these values work well
  byte ledBrightness = 220;  // Bright LED
  byte sampleAverage = 8;    // Average samples
  byte ledMode = 2;         // Red + IR mode
  byte sampleRate = 100;    // 100 samples per second
  int pulseWidth = 411;     // Pulse width
  int adcRange = 16384;     // ADC range
  
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  particleSensor.setPulseAmplitudeRed(ledBrightness);
  particleSensor.setPulseAmplitudeIR(ledBrightness);
  particleSensor.setPulseAmplitudeGreen(0);  // Turn off green
  
  // Ready beep
  make_beep(100);
  Serial.println("System ready!");
  Serial.println("Press button to start baseline");
  
  reset_to_idle();
}

void loop() {
  // Check button first
  check_button();
  
  // Read sensor data
  read_sensor_data();
  
  // Update display every 500ms
  if (millis() - last_display_update > 500) {
    update_display();
    last_display_update = millis();
  }
  
  // Handle different states
  if (is_idle) {
    handle_idle_state();
  }
  else if (capturing_baseline) {
    handle_baseline_capture();
  }
  else if (waiting_for_exercise) {
    handle_exercise_wait();
  }
  else if (timing_recovery) {
    handle_recovery_timing();
  }
  else if (session_complete) {
    handle_session_complete();
  }
  
  delay(10);  // Small delay to not overwhelm system
}

// Check button presses - basic debouncing
void check_button() {
  bool current_state = digitalRead(BUTTON_PIN);
  
  // Button just pressed
  if (current_state == LOW && last_button_state == HIGH) {
    button_press_time = millis();
    long_press_done = false;
  }
  
  // Button held down - check for long press
  if (current_state == LOW && !long_press_done) {
    if (millis() - button_press_time > 1500) {  // 1.5 second long press
      long_press_done = true;
      handle_long_press();
    }
  }
  
  // Button released - short press
  if (current_state == HIGH && last_button_state == LOW) {
    if (!long_press_done && millis() - button_press_time > 50) {  // 50ms debounce
      button_pressed = true;
    }
  }
  
  last_button_state = current_state;
}

// Handle long button press - export data
void handle_long_press() {
  if (is_idle) {
    Serial.println("\\n=== Session Data Export ===\");
    Serial.println("Session | Duration | Min SpO2 | Max HR | Baseline HR | Target Reached");
    
    for (int i = 0; i < session_count; i++) {
      Serial.print(i + 1);
      Serial.print(" | ");
      Serial.print(sessions[i].duration_seconds);
      Serial.print("s | ");
      Serial.print(sessions[i].min_spo2);
      Serial.print("% | ");
      Serial.print(sessions[i].max_heart_rate);
      Serial.print(" | ");
      Serial.print(sessions[i].baseline_hr);
      Serial.print(" | ");
      Serial.println(sessions[i].reached_target ? "Yes" : "No");
    }
    Serial.println("=== End Export ===\\n");
    
    // Double beep for export
    make_beep(200);
    delay(100);
    make_beep(200);
  }
}

// Read data from sensor
void read_sensor_data() {
  // Check if sensor has new data
  if (particleSensor.available()) {
    uint32_t red_value = particleSensor.getRed();
    uint32_t ir_value = particleSensor.getIR();
    particleSensor.nextSample();
    
    last_ir_reading = ir_value;  // For finger detection
    
    // Add to circular buffer
    red_buffer[buffer_position] = red_value;
    ir_buffer[buffer_position] = ir_value;
    buffer_position++;
    
    if (buffer_position >= BUFFER_SIZE) {
      buffer_position = 0;
      buffer_full = true;  // Buffer is now full
    }
  }
  
  // Calculate SpO2 and HR if buffer is full
  if (buffer_full && (millis() % 1000 < 50)) {  // Calculate roughly every second
    calculate_vitals();
  }
  
  particleSensor.check();  // Keep sensor alive
}

// Calculate SpO2 and heart rate using the algorithm
void calculate_vitals() {
  // Only calculate if finger is detected
  if (last_ir_reading < finger_threshold) {
    spo2_valid = 0;
    hr_valid = 0;
    return;
  }
  
  // Run the algorithm
  maxim_heart_rate_and_oxygen_saturation(ir_buffer, BUFFER_SIZE, red_buffer, &spo2_result, &spo2_valid, &hr_result, &hr_valid);
  
  // Simple smoothing - not too fancy
  if (spo2_valid && spo2_result > 0 && spo2_result <= 100) {
    if (smoothed_spo2 < 0) {
      smoothed_spo2 = spo2_result;  // First reading
    } else {
      // Simple averaging - mix 70% old with 30% new
      smoothed_spo2 = (smoothed_spo2 * 7 + spo2_result * 3) / 10;
    }
    current_spo2 = smoothed_spo2;
  }
  
  if (hr_valid && hr_result > 0 && hr_result < 200) {
    if (smoothed_hr < 0) {
      smoothed_hr = hr_result;  // First reading
    } else {
      // Simple averaging
      smoothed_hr = (smoothed_hr * 7 + hr_result * 3) / 10;
    }
    current_heart_rate = smoothed_hr;
  }
}

// Handle idle state
void handle_idle_state() {
  if (button_pressed) {
    button_pressed = false;
    
    // Check if finger is on sensor
    if (last_ir_reading > finger_threshold) {
      Serial.println("Starting baseline capture...");
      is_idle = false;
      capturing_baseline = true;
      baseline_start = millis();
      make_beep(100);  // Start beep
    } else {
      Serial.println("Put finger on sensor first!");
      make_beep(50);  // Error beep
    }
  }
}

// Handle baseline capture
void handle_baseline_capture() {
  // Cancel if button pressed
  if (button_pressed) {
    button_pressed = false;
    Serial.println("Cancelled baseline");
    make_beep(50);
    reset_to_idle();
    return;
  }
  
  // Check if time is up
  if (millis() - baseline_start >= baseline_time) {
    // Check if we have good readings
    if (current_spo2 > 0 && current_heart_rate > 0) {
      baseline_heart_rate = current_heart_rate;
      baseline_spo2_value = current_spo2;
      
      Serial.print("Baseline captured: HR = ");
      Serial.print(baseline_heart_rate);
      Serial.print(", SpO2 = ");
      Serial.println(baseline_spo2_value);
      
      // Move to exercise wait
      capturing_baseline = false;
      waiting_for_exercise = true;
      exercise_wait_start = millis();
      
      // Reset session tracking
      min_spo2_in_session = 100;
      max_hr_in_session = 0;
      
      // Double beep - baseline done
      make_beep(100);
      delay(100);
      make_beep(100);
      
      Serial.println("Now do some exercise and put finger back!");
    } else {
      Serial.println("Couldn't get good baseline readings");
      make_beep(500);  // Long error beep
      reset_to_idle();
    }
  }
}

// Handle waiting for exercise
void handle_exercise_wait() {
  // Check for timeout
  if (millis() - exercise_wait_start > exercise_timeout) {
    Serial.println("Exercise timeout - returning to idle");
    make_beep(1000);  // Long timeout beep
    reset_to_idle();
    return;
  }
  
  // Cancel with long press (handled elsewhere)
  // Short press or finger detection starts recovery
  if (button_pressed || (last_ir_reading > finger_threshold && current_heart_rate > 0)) {
    if (button_pressed) button_pressed = false;
    
    // Wait a bit for stable reading
    delay(2000);
    
    // Check if heart rate is elevated (shows exercise was done)
    if (current_heart_rate > baseline_heart_rate * 1.05 || button_pressed) {
      Serial.println("Starting recovery timing!");
      waiting_for_exercise = false;
      timing_recovery = true;
      recovery_start = millis();
      make_beep(100);  // Recovery start beep
    }
  }
}

// Handle recovery timing
void handle_recovery_timing() {
  // Track min/max values during recovery
  if (current_spo2 > 0 && current_spo2 < min_spo2_in_session) {
    min_spo2_in_session = current_spo2;
  }
  if (current_heart_rate > 0 && current_heart_rate > max_hr_in_session) {
    max_hr_in_session = current_heart_rate;
  }
  
  // Check if recovery targets are met
  bool spo2_recovered = false;
  bool hr_recovered = false;
  
  if (current_spo2 >= target_spo2) {
    spo2_recovered = true;
  }
  
  if (current_heart_rate > 0) {
    int hr_difference = abs(current_heart_rate - baseline_heart_rate);
    if (hr_difference <= (baseline_heart_rate * hr_recovery_percent)) {
      hr_recovered = true;
    }
  }
  
  bool target_reached = spo2_recovered && hr_recovered;
  
  // Manual stop or target reached or timeout
  if (button_pressed || target_reached || (millis() - recovery_start > max_session_time)) {
    button_pressed = false;
    session_end = millis();
    
    // Save session data
    save_session_data(target_reached);
    
    if (target_reached) {
      Serial.println("Recovery target reached!");
      // Triple beep for success
      make_beep(200);
      delay(100);
      make_beep(200);
      delay(100);
      make_beep(200);
    } else if (millis() - recovery_start > max_session_time) {
      Serial.println("Session timeout");
      make_beep(1000);  // Long timeout beep
    } else {
      Serial.println("Manual stop");
      make_beep(200);  // Single beep for manual stop
    }
    
    timing_recovery = false;
    session_complete = true;
  }
}

// Handle session complete state
void handle_session_complete() {
  if (button_pressed) {
    button_pressed = false;
    reset_to_idle();
  }
}

// Save session data to memory
void save_session_data(bool reached_target) {
  // Shift old sessions if array is full
  if (session_count >= 15) {
    for (int i = 0; i < 14; i++) {
      sessions[i] = sessions[i + 1];
    }
    session_count = 14;
  }
  
  // Add new session
  sessions[session_count].timestamp = millis();
  sessions[session_count].duration_seconds = (session_end - recovery_start) / 1000;
  sessions[session_count].min_spo2 = min_spo2_in_session;
  sessions[session_count].max_heart_rate = max_hr_in_session;
  sessions[session_count].baseline_hr = baseline_heart_rate;
  sessions[session_count].reached_target = reached_target;
  
  session_count++;
  
  Serial.print("Session saved: ");
  Serial.print(sessions[session_count-1].duration_seconds);
  Serial.print(" seconds, Min SpO2: ");
  Serial.print(sessions[session_count-1].min_spo2);
  Serial.print("%, Max HR: ");
  Serial.println(sessions[session_count-1].max_heart_rate);
}

// Reset everything back to idle
void reset_to_idle() {
  is_idle = true;
  capturing_baseline = false;
  waiting_for_exercise = false;
  timing_recovery = false;
  session_complete = false;
  
  // Clear some variables
  current_spo2 = -1;
  current_heart_rate = -1;
  smoothed_spo2 = -1;
  smoothed_hr = -1;
  
  Serial.println("Ready for next session");
}

// Update the OLED display
void update_display() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  
  // First line - current state
  if (is_idle) {
    display.println("READY - Press Start");
  }
  else if (capturing_baseline) {
    int time_left = (baseline_time - (millis() - baseline_start)) / 1000;
    display.print("BASELINE: ");
    display.print(time_left);
    display.println("s");
  }
  else if (waiting_for_exercise) {
    if (last_ir_reading > finger_threshold) {
      display.println("READY? Press or wait");
    } else {
      display.println("DO EXERCISE NOW!");
    }
  }
  else if (timing_recovery) {
    int elapsed = (millis() - recovery_start) / 1000;
    display.print("RECOVERY: ");
    display.print(elapsed);
    display.println("s");
  }
  else if (session_complete) {
    display.print("DONE! ");
    if (session_count > 0) {
      display.print(sessions[session_count-1].duration_seconds);
      display.println("s");
    }
  }
  
  // Second line - current readings
  if (!waiting_for_exercise || last_ir_reading > finger_threshold) {
    display.print("SpO2:");
    if (current_spo2 > 0) {
      display.print(current_spo2);
      display.print("%");
    } else {
      display.print("--");
    }
    
    display.print(" HR:");
    if (current_heart_rate > 0) {
      display.print(current_heart_rate);
      display.print("bpm");
    } else {
      display.print("---");
    }
  }
  
  // Third line - status info
  display.setCursor(0, 24);
  if (last_ir_reading < finger_threshold && !waiting_for_exercise) {
    display.print("No finger");
  }
  else if (timing_recovery && current_spo2 > 0 && current_heart_rate > 0) {
    display.print("Target: ");
    if (current_spo2 >= target_spo2) display.print("S");  // SpO2 good
    if (abs(current_heart_rate - baseline_heart_rate) <= baseline_heart_rate * hr_recovery_percent) {
      display.print("H");  // Heart rate good
    }
  }
  
  display.display();
}

// Make a beep sound
void make_beep(int duration_ms) {
  digitalWrite(buzzer_pin, HIGH);  // Turn on buzzer
  delay(duration_ms);
  digitalWrite(buzzer_pin, LOW);   // Turn off buzzer
}

// Debug function I used during testing
void print_debug_info() {
  Serial.print("IR: ");
  Serial.print(last_ir_reading);
  Serial.print(", SpO2: ");
  Serial.print(current_spo2);
  Serial.print(", HR: ");
  Serial.println(current_heart_rate);
}