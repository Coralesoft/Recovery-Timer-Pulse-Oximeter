/*
 * System Integration Test
 * 
 * This program tests all hardware components working together:
 * - MAX30102 sensor (SpO2 and heart rate)
 * - SSD1306 OLED display
 * - Push button input
 * - Buzzer audio output
 * 
 * Test sequence:
 * 1. Initialize all components
 * 2. Display system status on OLED
 * 3. Show live sensor readings
 * 4. Respond to button presses with beeps
 * 5. Demonstrate complete system functionality
 */

#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Hardware pins
#define I2C_SDA 21
#define I2C_SCL 22
#define BTN_PIN 25
#define BUZZER_PIN 15

// Display configuration
#define OLED_WIDTH 128
#define OLED_HEIGHT 32
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// Sensor
MAX30105 sensor;

// Test state
enum TestState {
  INIT_TEST,
  SENSOR_TEST,
  BUTTON_TEST,
  RUNNING_TEST
};
TestState currentTest = INIT_TEST;

// Button handling
bool lastButtonState = HIGH;
unsigned long lastButtonTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;

// Sensor data
uint32_t lastIR = 0;
bool fingerDetected = false;

void setup() {
  Serial.begin(115200);
  Serial.println("System Integration Test");
  Serial.println("======================");
  
  // Initialize I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  
  // Initialize GPIO
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Test sequence
  testDisplay();
  testSensor();
  testBuzzer();
  
  currentTest = RUNNING_TEST;
  Serial.println("All systems ready! Running integration test...");
  beep(100);
}

void loop() {
  switch(currentTest) {
    case RUNNING_TEST:
      readSensor();
      checkButton();
      updateDisplay();
      break;
    default:
      break;
  }
  
  delay(100);
}

void testDisplay() {
  Serial.print("Testing OLED display... ");
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("FAILED - Display not found at 0x3C");
    while(1) delay(1000);
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("System Test v1.0");
  display.println("Display: OK");
  display.display();
  
  Serial.println("OK");
  delay(1000);
}

void testSensor() {
  Serial.print("Testing MAX30102 sensor... ");
  
  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("FAILED - Sensor not found at 0x57");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Sensor: FAILED");
    display.display();
    while(1) delay(1000);
  }
  
  // Configure sensor
  sensor.setup(220, 8, 2, 100, 411, 16384);
  sensor.setPulseAmplitudeRed(220);
  sensor.setPulseAmplitudeIR(220);
  sensor.setPulseAmplitudeGreen(0);
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Display: OK");
  display.println("Sensor: OK");
  display.display();
  
  Serial.println("OK");
  delay(1000);
}

void testBuzzer() {
  Serial.print("Testing buzzer... ");
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Display: OK");
  display.println("Sensor: OK");
  display.println("Testing Buzzer...");
  display.display();
  
  // Test beep pattern
  for(int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
  
  Serial.println("OK");
  delay(1000);
}

void readSensor() {
  if (sensor.available()) {
    lastIR = sensor.getIR();
    sensor.nextSample();
  }
  sensor.check();
  
  fingerDetected = (lastIR > 15000);
}

void checkButton() {
  bool currentButtonState = digitalRead(BTN_PIN);
  
  if (currentButtonState != lastButtonState) {
    lastButtonTime = millis();
  }
  
  if ((millis() - lastButtonTime) > DEBOUNCE_DELAY) {
    if (currentButtonState == LOW && lastButtonState == HIGH) {
      // Button pressed
      Serial.println("Button pressed!");
      beep(200);
    }
  }
  
  lastButtonState = currentButtonState;
}

void updateDisplay() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 500) return; // Update every 500ms
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  
  display.println("System Integration");
  
  // Sensor status
  display.print("Sensor: ");
  if (fingerDetected) {
    display.print("FINGER OK");
  } else {
    display.print("No finger");
  }
  display.println();
  
  // Button status
  display.print("Button: ");
  if (digitalRead(BTN_PIN) == LOW) {
    display.print("PRESSED");
  } else {
    display.print("Ready");
  }
  display.println();
  
  // Raw IR value for debugging
  display.print("IR: ");
  display.print(lastIR);
  
  display.display();
  lastUpdate = millis();
}

void beep(int duration) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
}