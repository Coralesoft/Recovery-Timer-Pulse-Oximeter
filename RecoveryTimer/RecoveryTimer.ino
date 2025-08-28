/*
 * Recovery Timer Project - Year 12 Electronics
 * Made by: Max Brown
 * Wellington College 2025
 * 
 * This project measures how fast your heart rate and oxygen levels return to normal after exercise
 * I got the idea from fitness trackers but wanted to make my own version
 * 
 * Components:
 * - ESP32 microcontroller 
 * - MAX30102 heart rate and SpO2 sensor
 * - 0.91" OLED display (128x32)
 * - Push button for control
 * - Buzzer for audio feedback
 * 
 * How it works:
 * 1. Press button to start - captures your resting heart rate and oxygen for 10 seconds
 * 2. Remove finger and do some exercise (jumping jacks, run up stairs etc)
 * 3. Put finger back on sensor - it times how long to get back to normal levels
 * 4. Shows your recovery time and saves the data
 * 
 * TODO: Maybe add WiFi to send data to phone
 * TODO: Make the readings more stable - they jump around sometimes
 */

// Include necessary libraries for the project
#include <Wire.h>              // This library lets us communicate with I2C devices (sensor and display)
#include "MAX30105.h"          // SparkFun library for the MAX30102 heart rate/oxygen sensor
#include "spo2_algorithm.h"    // Math algorithm that calculates oxygen levels from sensor data
#include <Adafruit_GFX.h>      // Graphics library that helps draw on the OLED screen
#include <Adafruit_SSD1306.h>  // Specific library for our SSD1306 OLED display

// Pin definitions - these tell the ESP32 which pins we connected our components to
#define SDA_pin 21       // I2C data pin - sends data between ESP32 and devices
#define SCL_pin 22       // I2C clock pin - keeps communication in sync
#define button_pin 25    // The pin our push button is connected to
#define BUZZER 15        // The pin our buzzer is connected to for beeping sounds

// Display setup - defines the size and settings for our OLED screen
#define screen_width 128     // Our OLED is 128 pixels wide
#define screen_height 32     // Our OLED is 32 pixels tall
#define OLED_RESET -1        // -1 means we don't use a reset pin for the display
#define OLED_ADDR 0x3C       // The I2C address of our OLED (most use 0x3C)

// Create an object to control the display using the settings above
Adafruit_SSD1306 display(screen_width, screen_height, &Wire, OLED_RESET);

// Create an object to control the heart rate sensor
MAX30105 heartSensor;

// Program state flags - these track what the device is currently doing
// Using simple boolean flags instead of complex state machines
bool idle_mode = true;           // True when waiting for user to start
bool doing_baseline = false;     // True when capturing resting heart rate
bool waiting_for_exercise = false; // True when user should be exercising
bool timing_recovery = false;    // True when measuring recovery time
bool showing_results = false;    // True when displaying final results

// Button handling variables - used to detect short and long button presses
bool button_state = HIGH;        // Current state of button (HIGH = not pressed)
bool last_button_state = HIGH;   // What the button was last time we checked
unsigned long button_press_start = 0;  // When the button press started (for timing)
bool long_press_handled = false; // Prevents triggering long press multiple times

// Timing variables - keep track of when different phases start
unsigned long baseline_start_time = 0;    // When we started measuring baseline
unsigned long exercise_wait_start = 0;    // When we started waiting for exercise
unsigned long recovery_start_time = 0;    // When we started timing recovery
unsigned long recovery_end_time = 0;      // When recovery ended

// Baseline readings - the user's normal resting values
int baseline_heart_rate = 0;  // Normal heart rate before exercise
int baseline_oxygen = 0;      // Normal oxygen level before exercise

// Current readings from sensor - what we're measuring right now
int current_heart_rate = -1;    // Current heart rate (-1 means no valid reading)
int current_oxygen = -1;         // Current oxygen level (-1 means no valid reading)
int smoothed_heart_rate = -1;   // Smoothed version to reduce noise
int smoothed_oxygen = -1;       // Smoothed version to reduce noise

// Session tracking - records the min/max values during recovery
int lowest_oxygen = 100;        // Tracks the lowest oxygen level seen
int highest_heart_rate = 0;     // Tracks the highest heart rate seen

// Data storage structure - defines what information we save for each session
struct session_record {
  unsigned long when_it_happened;  // Timestamp of when session happened
  int how_long_seconds;            // How long recovery took in seconds
  int lowest_spo2;                 // The lowest oxygen level during recovery
  int highest_hr;                  // The highest heart rate during recovery
  int resting_hr;                  // What the resting heart rate was
  bool auto_stopped;               // Did it stop automatically (true) or manually (false)
};

// Array to store up to 20 exercise sessions
session_record saved_sessions[20];
int total_sessions = 0;  // How many sessions we've recorded

// Algorithm variables - the sensor algorithm needs 100 samples to work properly
#define BUFFER_SIZE 100    // We need to collect 100 samples for accurate calculations

// Arrays to store the red and infrared light readings from the sensor
uint32_t red_values[BUFFER_SIZE];   // Red LED readings
uint32_t ir_values[BUFFER_SIZE];    // Infrared LED readings
int buffer_pos = 0;                 // Current position in the buffer
bool got_enough_data = false;       // True when we have 100 samples

// Variables to store the calculated results from the algorithm
int32_t calculated_spo2;        // The oxygen level calculated by algorithm
int8_t spo2_is_valid;           // Whether the oxygen calculation is reliable
int32_t calculated_hr;          // The heart rate calculated by algorithm
int8_t hr_is_valid;             // Whether the heart rate calculation is reliable
uint32_t latest_ir_reading = 0; // Most recent infrared reading (for finger detection)

// Settings and thresholds - these control how the device behaves
int finger_detected_threshold = 6500;     // If IR reading > this, finger is on sensor
int target_oxygen_level = 96;             // Recovery target: oxygen should be >= 96%
float heart_rate_recovery_margin = 0.12;  // Recovery target: HR within 12% of baseline
unsigned long baseline_capture_time = 10000;   // Capture baseline for 10 seconds (10000ms)
unsigned long max_recovery_time = 300000;      // Maximum recovery time is 5 minutes
unsigned long exercise_timeout_time = 120000;  // Give user 2 minutes to start exercise

// Display timing - controls how often we update the screen
unsigned long last_screen_update = 0;  // When we last updated the display

// Debug variables I used while testing (not used in final version)
// bool show_debug_info = false;   // Toggle debug output
// int loop_counter = 0;           // Count how many times loop() runs

// Setup function - runs once when the device starts
void setup() {
  // Start serial communication for debugging messages
  Serial.begin(115200);  // 115200 is the speed of communication
  Serial.println("Recovery Timer Starting...");
  Serial.println("Made by Max Brown - Year 12 Electronics");
  
  // Setup the pins - tell the ESP32 how to use each pin
  pinMode(button_pin, INPUT_PULLUP);  // Button uses internal pullup resistor
  pinMode(BUZZER, OUTPUT);            // Buzzer is an output device
  digitalWrite(BUZZER, LOW);          // Make sure buzzer starts silent
  
  // Start I2C communication bus for sensor and display
  Wire.begin(SDA_pin, SCL_pin);  // Tell ESP32 which pins to use for I2C
  Wire.setClock(400000);          // Set I2C speed to 400kHz (fast mode)
  
  // Try to initialize the OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    // If display not found, print error and flash LED
    Serial.println("ERROR: Could not find OLED display!");
    // Flash the built-in LED to show error
    while(1) {  // Loop forever
      digitalWrite(2, HIGH);  // LED on
      delay(500);            // Wait 500ms
      digitalWrite(2, LOW);   // LED off
      delay(500);            // Wait 500ms
    }
  }
  
  Serial.println("OLED display found and working");
  
  // Show startup screen on the display
  display.clearDisplay();              // Clear any old content
  display.setTextSize(1);             // Small text size
  display.setTextColor(SSD1306_WHITE); // White text color
  display.setCursor(0,0);              // Start at top-left corner
  display.println("Recovery Timer");    // Print title
  display.println("Year 12 Project");   // Print subtitle
  display.println("by Max Brown");      // Print author
  display.display();                    // Actually show on screen
  delay(5000);                         // Show for 5 seconds
  
  // Try to initialize the heart rate sensor
  if (!heartSensor.begin(Wire, I2C_SPEED_FAST)) {
    // If sensor not found, show error message
    Serial.println("ERROR: Could not find MAX30102 sensor!");
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Sensor Not Found!");
    display.println("Check wiring:");
    display.println("VIN->3V3, GND->GND");
    display.println("SDA->21, SCL->22");
    display.display();
    while(1) delay(1000);  // Stop here forever if no sensor
  }
  
  Serial.println("Heart rate sensor found!");
  
  // Configure the sensor settings - these values work well after testing
  byte ledPower = 200;      // LED brightness (0-255, 200 is bright but not max)
  byte avgSamples = 8;      // Average 8 samples to reduce noise
  byte ledMode = 2;         // Mode 2 = Red + Infrared LEDs (needed for oxygen)
  byte sampleRate = 100;    // Take 100 samples per second
  int pulseWidth = 411;     // How long each LED pulse lasts (in microseconds)
  int adcRange = 16384;     // Analog-to-digital converter resolution
  
  // Apply all the settings to the sensor
  heartSensor.setup(ledPower, avgSamples, ledMode, sampleRate, pulseWidth, adcRange);
  heartSensor.setPulseAmplitudeRed(ledPower);   // Set red LED brightness
  heartSensor.setPulseAmplitudeIR(ledPower);    // Set infrared LED brightness
  heartSensor.setPulseAmplitudeGreen(0);        // Turn off green LED (not needed)
  
  // Make a startup beep to show everything is working
  digitalWrite(BUZZER, HIGH);  // Turn buzzer on
  delay(150);                  // Beep for 150ms
  digitalWrite(BUZZER, LOW);   // Turn buzzer off
  
  Serial.println("System ready! Place finger on sensor and press button to start.");
  
  // Start in idle state, ready for user
  go_to_idle_state();
}

// Main loop function - runs continuously after setup()
void loop() {
  // Check if the button was pressed
  handle_button_input();
  
  // Read new data from the heart rate sensor
  read_sensor_data();
  
  // Update the display every 500ms (twice per second)
  // This prevents flickering from updating too fast
  if (millis() - last_screen_update > 500) {
    update_display();
    last_screen_update = millis();
  }
  
  // Handle the current state - only one of these will be true at a time
  if (idle_mode) {
    handle_idle_mode();              // Waiting for user to start
  }
  else if (doing_baseline) {
    handle_baseline_capture();       // Capturing resting values
  }
  else if (waiting_for_exercise) {
    handle_waiting_for_exercise();   // Waiting for user to exercise
  }
  else if (timing_recovery) {
    handle_recovery_timing();        // Timing the recovery
  }
  else if (showing_results) {
    handle_showing_results();        // Showing final results
  }
  
  // Small delay to prevent overwhelming the system
  delay(20);  // 20ms delay = loop runs about 50 times per second
}

// Function to check for button presses with debouncing
// Debouncing prevents false triggers from electrical noise
void handle_button_input() {
  // Read the current state of the button
  button_state = digitalRead(button_pin);
  
  // Check if button just got pressed (went from HIGH to LOW)
  if (button_state == LOW && last_button_state == HIGH) {
    button_press_start = millis();  // Record when press started
    long_press_handled = false;     // Reset long press flag
    Serial.println("Button pressed");
  }
  
  // Check if button is being held down (for long press)
  if (button_state == LOW && !long_press_handled) {
    // Check if button has been held for 2 seconds
    if (millis() - button_press_start > 2000) {
      long_press_handled = true;  // Mark as handled so it doesn't repeat
      handle_long_button_press();  // Do long press action
    }
  }
  
  // Check if button was just released (went from LOW to HIGH)
  if (button_state == HIGH && last_button_state == LOW) {
    // If it wasn't a long press and was pressed for at least 50ms
    if (!long_press_handled && millis() - button_press_start > 50) {
      handle_short_button_press();  // Do short press action
    }
  }
  
  // Remember the button state for next time
  last_button_state = button_state;
}

// Function to handle short button presses - main control
void handle_short_button_press() {
  Serial.println("Short button press detected");
  
  // Different actions depending on current state
  if (idle_mode) {
    // In idle mode: start baseline capture if finger detected
    if (latest_ir_reading > finger_detected_threshold) {
      Serial.println("Starting baseline capture");
      start_baseline_capture();
    } else {
      Serial.println("No finger detected - place finger on sensor first");
      error_beep();  // Make error sound
    }
  }
  else if (doing_baseline) {
    // During baseline: cancel and go back to idle
    Serial.println("Cancelling baseline capture");
    cancel_beep();
    go_to_idle_state();
  }
  else if (waiting_for_exercise) {
    // While waiting: manually start recovery timing
    if (latest_ir_reading > finger_detected_threshold) {
      Serial.println("Manual start of recovery timing");
      start_recovery_timing();
    }
  }
  else if (timing_recovery) {
    // During recovery: manually stop timing
    Serial.println("Manual stop of recovery timing");
    stop_recovery_timing(false);  // false = manual stop, not automatic
  }
  else if (showing_results) {
    // When showing results: go back to idle
    Serial.println("Going back to idle");
    go_to_idle_state();
  }
}

// Function to handle long button presses - special features
void handle_long_button_press() {
  Serial.println("Long button press detected");
  
  if (idle_mode) {
    // In idle mode: export all saved session data to serial monitor
    Serial.println("\n=== Session Data Export ===");
    Serial.println("Session | Time | Min O2 | Max HR | Rest HR | Auto Stop");
    Serial.println("--------|------|--------|--------|---------|----------");
    
    // Loop through all saved sessions and print them
    for (int i = 0; i < total_sessions; i++) {
      Serial.print(i + 1);                // Session number
      Serial.print("       | ");
      Serial.print(saved_sessions[i].how_long_seconds);  // Recovery time
      Serial.print("s   | ");
      Serial.print(saved_sessions[i].lowest_spo2);       // Minimum oxygen
      Serial.print("%     | ");
      Serial.print(saved_sessions[i].highest_hr);        // Maximum heart rate
      Serial.print("     | ");
      Serial.print(saved_sessions[i].resting_hr);        // Baseline heart rate
      Serial.print("      | ");
      Serial.println(saved_sessions[i].auto_stopped ? "Yes" : "No");  // How it ended
    }
    Serial.println("=== End Export ===\n");
    
    // Double beep to confirm data was exported
    success_beep();
    delay(200);
    success_beep();
  }
  else if (waiting_for_exercise) {
    // While waiting for exercise: cancel and go back
    Serial.println("Cancelling exercise wait");
    cancel_beep();
    go_to_idle_state();
  }
}

// Function to read data from the MAX30102 sensor
void read_sensor_data() {
  // Check if the sensor has new data ready
  if (heartSensor.available()) {
    // Read the red and infrared light values
    uint32_t red_reading = heartSensor.getRed();
    uint32_t ir_reading = heartSensor.getIR();
    heartSensor.nextSample();  // Tell sensor to prepare next sample
    
    // Save IR reading for finger detection
    latest_ir_reading = ir_reading;
    
    // Store readings in our circular buffer for the algorithm
    red_values[buffer_pos] = red_reading;
    ir_values[buffer_pos] = ir_reading;
    buffer_pos++;  // Move to next position in buffer
    
    // When buffer is full, wrap back to beginning
    if (buffer_pos >= BUFFER_SIZE) {
      buffer_pos = 0;  // Start over at beginning
      got_enough_data = true;  // Now we have enough data for calculations
    }
  }
  
  // Calculate heart rate and oxygen every second
  static unsigned long last_calculation = 0;  // Remember when we last calculated
  if (got_enough_data && millis() - last_calculation > 1000) {
    calculate_heart_rate_and_oxygen();  // Do the calculation
    last_calculation = millis();        // Remember this time
  }
  
  // Keep sensor communication active
  heartSensor.check();
}

// Function to calculate heart rate and oxygen from sensor data
void calculate_heart_rate_and_oxygen() {
  // First check if finger is on the sensor
  if (latest_ir_reading < finger_detected_threshold) {
    // No finger detected - set everything to invalid
    spo2_is_valid = 0;
    hr_is_valid = 0;
    current_heart_rate = -1;
    current_oxygen = -1;
    return;  // Exit function early
  }
  
  // Run the SpO2 algorithm from Maxim (the sensor manufacturer)
  // This complex math converts light readings into heart rate and oxygen
  maxim_heart_rate_and_oxygen_saturation(
    ir_values, BUFFER_SIZE, red_values,  // Input: our collected samples
    &calculated_spo2, &spo2_is_valid,    // Output: oxygen level and validity
    &calculated_hr, &hr_is_valid         // Output: heart rate and validity
  );
  
  // Apply smoothing to oxygen reading to reduce noise
  if (spo2_is_valid && calculated_spo2 > 0 && calculated_spo2 <= 100) {
    if (smoothed_oxygen < 0) {
      // First reading - just use it directly
      smoothed_oxygen = calculated_spo2;
    } else {
      // Weighted average: 80% old value + 20% new value
      // This makes readings more stable
      smoothed_oxygen = (smoothed_oxygen * 8 + calculated_spo2 * 2) / 10;
    }
    current_oxygen = smoothed_oxygen;  // Save the smoothed value
  }
  
  // Apply smoothing to heart rate reading
  if (hr_is_valid && calculated_hr > 0 && calculated_hr < 200) {
    if (smoothed_heart_rate < 0) {
      // First reading - use directly
      smoothed_heart_rate = calculated_hr;
    } else {
      // Weighted average for stability
      smoothed_heart_rate = (smoothed_heart_rate * 8 + calculated_hr * 2) / 10;
    }
    current_heart_rate = smoothed_heart_rate;  // Save the smoothed value
  }
  
  // Debug output - uncomment these lines if having problems
  // Serial.print("IR: "); Serial.print(latest_ir_reading);
  // Serial.print(" HR: "); Serial.print(current_heart_rate);
  // Serial.print(" O2: "); Serial.println(current_oxygen);
}

// Function to handle idle state - just waiting
void handle_idle_mode() {
  // Nothing special to do in idle mode
  // We just wait for button press (handled in handle_button_input)
}

// Function to start capturing baseline readings
void start_baseline_capture() {
  // Change state flags
  idle_mode = false;
  doing_baseline = true;
  baseline_start_time = millis();  // Record start time
  
  // Reset smoothing values for fresh start
  smoothed_heart_rate = -1;
  smoothed_oxygen = -1;
  
  // Make a beep to indicate start
  start_beep();
}

// Function to handle baseline capture state
void handle_baseline_capture() {
  // Check if 10 seconds have passed
  if (millis() - baseline_start_time >= baseline_capture_time) {
    // Time's up - check if we got valid readings
    if (current_heart_rate > 0 && current_oxygen > 0) {
      // Save the baseline values
      baseline_heart_rate = current_heart_rate;
      baseline_oxygen = current_oxygen;
      
      // Print baseline to serial monitor
      Serial.print("Baseline captured: HR = ");
      Serial.print(baseline_heart_rate);
      Serial.print(" bpm, O2 = ");
      Serial.print(baseline_oxygen);
      Serial.println("%");
      
      // Move to next state - waiting for exercise
      doing_baseline = false;
      waiting_for_exercise = true;
      exercise_wait_start = millis();  // Start timing the wait
      
      // Reset session tracking variables
      lowest_oxygen = 100;      // Start with max possible
      highest_heart_rate = 0;   // Start with minimum
      
      // Double beep for success
      success_beep();
      delay(150);
      success_beep();
      
      Serial.println("Now remove finger, do some exercise, then put finger back!");
    } else {
      // Couldn't get good readings - go back to idle
      Serial.println("Could not get stable baseline readings - try again");
      error_beep();
      go_to_idle_state();
    }
  }
}

// Function to handle waiting for user to exercise
void handle_waiting_for_exercise() {
  // Check if user is taking too long (2 minute timeout)
  if (millis() - exercise_wait_start > exercise_timeout_time) {
    Serial.println("Exercise timeout - going back to idle");
    timeout_beep();  // Long beep for timeout
    go_to_idle_state();
    return;  // Exit function
  }
  
  // Check if finger is back on sensor with valid heart rate
  if (latest_ir_reading > finger_detected_threshold && current_heart_rate > 0) {
    // Finger detected - wait for stable reading
    static unsigned long finger_return_time = 0;  // Static = remembers between calls
    if (finger_return_time == 0) {
      finger_return_time = millis();  // Start timing
    }
    
    // After 3 seconds of stable finger placement
    if (millis() - finger_return_time > 3000) {
      // Check if heart rate is elevated (shows exercise was done)
      if (current_heart_rate > baseline_heart_rate * 1.08) {  // 8% higher
        Serial.println("Exercise detected! Starting recovery timing");
        start_recovery_timing();
        finger_return_time = 0;  // Reset for next time
      }
    }
  } else {
    // Finger not detected - reset timer
    static unsigned long finger_return_time = 0;
    finger_return_time = 0;
  }
}

// Function to start timing the recovery
void start_recovery_timing() {
  // Change state flags
  waiting_for_exercise = false;
  timing_recovery = true;
  recovery_start_time = millis();  // Record start time
  
  // Make a beep to indicate start
  start_beep();
}

// Function to handle recovery timing state
void handle_recovery_timing() {
  // Track the lowest oxygen level seen during recovery
  if (current_oxygen > 0 && current_oxygen < lowest_oxygen) {
    lowest_oxygen = current_oxygen;
  }
  
  // Track the highest heart rate seen during recovery
  if (current_heart_rate > 0 && current_heart_rate > highest_heart_rate) {
    highest_heart_rate = current_heart_rate;
  }
  
  // Check if recovery targets are met
  bool oxygen_recovered = false;
  bool heart_rate_recovered = false;
  
  // Check oxygen target (should be 96% or higher)
  if (current_oxygen >= target_oxygen_level) {
    oxygen_recovered = true;
  }
  
  // Check heart rate target (should be close to baseline)
  if (current_heart_rate > 0) {
    // Calculate how different current HR is from baseline
    int hr_difference = abs(current_heart_rate - baseline_heart_rate);
    // Calculate acceptable difference (12% of baseline)
    int acceptable_difference = baseline_heart_rate * heart_rate_recovery_margin;
    // Check if within acceptable range
    if (hr_difference <= acceptable_difference) {
      heart_rate_recovered = true;
    }
  }
  
  // Check if both targets are met
  if (oxygen_recovered && heart_rate_recovered) {
    Serial.println("Recovery targets reached! Auto-stopping timer");
    stop_recovery_timing(true);  // true = automatic stop (targets reached)
    return;
  }
  
  // Check for timeout (5 minutes max)
  if (millis() - recovery_start_time > max_recovery_time) {
    Serial.println("Recovery timeout - stopping timer");
    stop_recovery_timing(false);  // false = timeout, targets not reached
  }
}

// Function to stop recovery timing and save results
void stop_recovery_timing(bool reached_targets) {
  // Change state flags
  timing_recovery = false;
  showing_results = true;
  recovery_end_time = millis();  // Record end time
  
  // Calculate how long recovery took
  int recovery_seconds = (recovery_end_time - recovery_start_time) / 1000;
  
  // Save this session's data
  save_session_data(recovery_seconds, reached_targets);
  
  // Different beep patterns for different outcomes
  if (reached_targets) {
    // Triple beep for successful recovery
    success_beep();
    delay(200);
    success_beep();
    delay(200);
    success_beep();
    Serial.println("Congratulations! You reached your recovery targets!");
  } else {
    // Single long beep for manual stop or timeout
    cancel_beep();
    Serial.println("Recovery session ended");
  }
  
  // Print results to serial monitor
  Serial.print("Recovery time: ");
  Serial.print(recovery_seconds);
  Serial.print(" seconds, Min O2: ");
  Serial.print(lowest_oxygen);
  Serial.print("%, Max HR: ");
  Serial.print(highest_heart_rate);
  Serial.println(" bpm");
}

// Function to save session data to memory
void save_session_data(int duration, bool auto_stopped) {
  // If array is full, shift everything left to make room
  if (total_sessions >= 20) {
    // Move sessions 1-19 to positions 0-18
    for (int i = 0; i < 19; i++) {
      saved_sessions[i] = saved_sessions[i + 1];
    }
    total_sessions = 19;  // Now we have room at position 19
  }
  
  // Add new session data to the end of array
  saved_sessions[total_sessions].when_it_happened = millis();
  saved_sessions[total_sessions].how_long_seconds = duration;
  saved_sessions[total_sessions].lowest_spo2 = lowest_oxygen;
  saved_sessions[total_sessions].highest_hr = highest_heart_rate;
  saved_sessions[total_sessions].resting_hr = baseline_heart_rate;
  saved_sessions[total_sessions].auto_stopped = auto_stopped;
  
  total_sessions++;  // Increment counter
  
  // Confirm save to serial monitor
  Serial.print("Session saved (total sessions: ");
  Serial.print(total_sessions);
  Serial.println(")");
}

// Function to handle showing results state
void handle_showing_results() {
  // Nothing special to do - just wait for button press
  // Button press will return to idle (handled in handle_button_input)
}

// Function to reset everything back to idle state
void go_to_idle_state() {
  // Reset all state flags
  idle_mode = true;
  doing_baseline = false;
  waiting_for_exercise = false;
  timing_recovery = false;
  showing_results = false;
  
  // Clear current readings
  current_heart_rate = -1;
  current_oxygen = -1;
  smoothed_heart_rate = -1;
  smoothed_oxygen = -1;
  
  Serial.println("Back to idle mode - ready for next session");
}

// Function to update the OLED display
void update_display() {
  display.clearDisplay();  // Clear old content
  display.setTextSize(1);  // Small text
  display.setTextColor(SSD1306_WHITE);  // White text
  display.setCursor(0, 0);  // Start at top-left
  
  // First line - show what state we're in
  if (idle_mode) {
    display.println("READY- Press to start");
  }
  else if (doing_baseline) {
    // Show countdown during baseline capture
    int time_left = (baseline_capture_time - (millis() - baseline_start_time)) / 1000;
    display.print("BASELINE: ");
    display.print(time_left + 1);  // Add 1 so it doesn't show 0
    display.println(" seconds");
  }
  else if (waiting_for_exercise) {
    // Different message depending on finger detection
    if (latest_ir_reading > finger_detected_threshold) {
      display.println("Ready to start timing?");
    } else {
      display.println("Do exercise, then");
      display.println("place finger back!");
    }
  }
  else if (timing_recovery) {
    // Show elapsed recovery time
    int elapsed = (millis() - recovery_start_time) / 1000;
    display.print("RECOVERY: ");
    display.print(elapsed);
    display.println(" sec");
  }
  else if (showing_results) {
    // Show final recovery time
    display.print("DONE! Time: ");
    if (total_sessions > 0) {
      display.print(saved_sessions[total_sessions-1].how_long_seconds);
      display.println("s");
    }
  }
  
  // Second line - show current readings (except when waiting for exercise without finger)
  if (!waiting_for_exercise || latest_ir_reading > finger_detected_threshold) {
    display.print("O2: ");
    if (current_oxygen > 0) {
      display.print(current_oxygen);
      display.print("%");
    } else {
      display.print("--");  // No valid reading
    }
    
    display.print("  HR: ");
    if (current_heart_rate > 0) {
      display.print(current_heart_rate);
      display.print("bpm");
    } else {
      display.print("---");  // No valid reading
    }
    display.println();
  }
  
  // Third line - status information
  if (latest_ir_reading < finger_detected_threshold && !waiting_for_exercise) {
    // Remind user to place finger (shortened to fit display width)
    display.println("Place finger on");
  }
  else if (timing_recovery && current_oxygen > 0 && current_heart_rate > 0) {
    // Show which recovery targets are met (using simple text instead of special characters)
    display.print("Target: ");
    
    // Check oxygen target
    if (current_oxygen >= target_oxygen_level) {
      display.print("O2-OK ");  // Using simple text instead of checkmark
    } else {
      display.print("O2-NO ");  // Using simple text instead of X
    }
    
    // Check heart rate target
    int hr_diff = abs(current_heart_rate - baseline_heart_rate);
    int acceptable_diff = baseline_heart_rate * heart_rate_recovery_margin;
    if (hr_diff <= acceptable_diff) {
      display.print("HR-OK");   // Using simple text instead of checkmark
    } else {
      display.print("HR-NO");   // Using simple text instead of X
    }
  }
  
  display.display();  // Send everything to the actual display
}

// Sound functions for different types of user feedback

// Short beep for starting something
void start_beep() {
  digitalWrite(BUZZER, HIGH);  // Turn buzzer on
  delay(100);                  // Stay on for 100ms
  digitalWrite(BUZZER, LOW);   // Turn buzzer off
}

// Medium beep for success
void success_beep() {
  digitalWrite(BUZZER, HIGH);
  delay(200);                  // 200ms beep
  digitalWrite(BUZZER, LOW);
}

// Very short beep for errors
void error_beep() {
  digitalWrite(BUZZER, HIGH);
  delay(50);                   // Only 50ms beep
  digitalWrite(BUZZER, LOW);
}

// Longer beep for cancelling
void cancel_beep() {
  digitalWrite(BUZZER, HIGH);
  delay(300);                  // 300ms beep
  digitalWrite(BUZZER, LOW);
}

// Very long beep for timeout
void timeout_beep() {
  digitalWrite(BUZZER, HIGH);
  delay(1000);                 // 1 second beep
  digitalWrite(BUZZER, LOW);
}

// Debug function I used during development - prints all values
void print_debug_values() {
  // Print current state
  Serial.print("State: ");
  if (idle_mode) Serial.print("IDLE");
  else if (doing_baseline) Serial.print("BASELINE");
  else if (waiting_for_exercise) Serial.print("WAITING");
  else if (timing_recovery) Serial.print("TIMING");
  else if (showing_results) Serial.print("RESULTS");
  
  // Print sensor values
  Serial.print(" | IR: ");
  Serial.print(latest_ir_reading);
  Serial.print(" | HR: ");
  Serial.print(current_heart_rate);
  Serial.print(" | O2: ");
  Serial.print(current_oxygen);
  Serial.println();
}