/*
 * MAX30102 Raw Data Test Program
 * 
 * This test program verifies basic communication with the MAX30102 sensor
 * and outputs raw IR/Red values to confirm the sensor is working properly.
 * 
 * Expected behavior:
 * - Shows "MAX30102 Found" if sensor is detected at I2C address 0x57
 * - Continuously outputs IR and Red values
 * - Values should change significantly when finger is placed on sensor
 * - IR values typically 5000-50000 with finger, <1000 without
 * 
 * Wiring:
 * - MAX30102 VIN → ESP32 3V3
 * - MAX30102 GND → ESP32 GND  
 * - MAX30102 SDA → ESP32 GPIO21 (D21)
 * - MAX30102 SCL → ESP32 GPIO22 (D22)
 * - MAX30102 INT → ESP32 GPIO19 (D19) [optional]
 */

#include <Wire.h>
#include "MAX30105.h"

// I2C pin configuration
#define I2C_SDA 21
#define I2C_SCL 22

// Create sensor object
MAX30105 sensor;

void setup() {
  Serial.begin(115200);
  Serial.println("MAX30102 Raw Data Test");
  
  // Initialize I2C with custom pins
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000); // 400kHz I2C speed
  
  // Initialize sensor
  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("ERROR: MAX30102 not found at I2C address 0x57");
    Serial.println("Check wiring:");
    Serial.println("  VIN → 3V3");
    Serial.println("  GND → GND");
    Serial.println("  SDA → GPIO21");
    Serial.println("  SCL → GPIO22");
    while(1) delay(1000);
  }
  
  Serial.println("MAX30102 Found!");
  
  // Configure sensor for basic operation
  byte ledBrightness = 220;  // Bright enough for good signal
  byte sampleAverage = 4;    // Average 4 samples
  byte ledMode = 2;          // Red + IR mode (needed for SpO2)
  byte sampleRate = 100;     // 100 samples per second
  int pulseWidth = 411;      // Pulse width in microseconds
  int adcRange = 16384;      // ADC range
  
  sensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  
  Serial.println("Sensor configured. Place finger on sensor...");
  Serial.println("Format: IR_Value, Red_Value");
  Serial.println("Expected: >15000 with finger, <1000 without finger");
  Serial.println();
}

void loop() {
  // Check if sensor has new data
  if (sensor.available()) {
    // Read the raw values
    uint32_t irValue = sensor.getIR();
    uint32_t redValue = sensor.getRed();
    
    // Move to next sample
    sensor.nextSample();
    
    // Output values in CSV format for easy analysis
    Serial.print(irValue);
    Serial.print(", ");
    Serial.print(redValue);
    
    // Add status indicator
    if (irValue > 15000) {
      Serial.println(" [FINGER DETECTED]");
    } else {
      Serial.println(" [No finger]");
    }
    
    delay(100); // Limit output rate for readability
  }
  
  // Keep sensor communication alive
  sensor.check();
}