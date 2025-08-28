/*
 * Pulse Oximeter Wrapper Library Test
 * 
 * This program tests the PulseOximeterWrapper library to ensure it works correctly
 * before using it in the main recovery timer program.
 * 
 * Expected behavior:
 * - Shows "Wrapper initialized" if library loads successfully
 * - Displays live SpO₂ and heart rate when finger is placed on sensor
 * - Shows "No finger" when finger is removed
 * - Readings should be stable and within normal ranges (SpO₂: 95-100%, HR: 60-100 at rest)
 */

#include <Wire.h>
#include "PulseOximeterWrapper.h"

// I2C Configuration  
#define I2C_SDA 21
#define I2C_SCL 22

// Create wrapper object
PulseOximeterWrapper oximeter;

void setup() {
  Serial.begin(115200);
  Serial.println("Pulse Oximeter Wrapper Test");
  Serial.println("===========================");
  
  // Initialize I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  
  // Initialize wrapper
  if (!oximeter.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("ERROR: Failed to initialize pulse oximeter wrapper");
    Serial.println("Check sensor wiring and power");
    while(1) delay(1000);
  }
  
  Serial.println("Wrapper initialized successfully!");
  Serial.println("Place finger on sensor for readings...");
  Serial.println("Format: SpO2%, HeartRate BPM, Status");
  Serial.println();
}

void loop() {
  // Update wrapper (handles all sensor processing)
  oximeter.update();
  
  // Display results every second
  static unsigned long lastDisplay = 0;
  if (millis() - lastDisplay >= 1000) {
    displayReadings();
    lastDisplay = millis();
  }
  
  // Small delay
  delay(10);
}

void displayReadings() {
  Serial.print("SpO2: ");
  if (oximeter.isSpO2Valid()) {
    Serial.print(oximeter.getSpO2());
    Serial.print("%");
  } else {
    Serial.print("--");
  }
  
  Serial.print(" | HR: ");
  if (oximeter.isHeartRateValid()) {
    Serial.print(oximeter.getHeartRate());
    Serial.print(" BPM");
  } else {
    Serial.print("--- BPM");
  }
  
  Serial.print(" | Status: ");
  if (!oximeter.isFingerDetected()) {
    Serial.println("No finger detected");
  } else if (oximeter.isSpO2Valid() && oximeter.isHeartRateValid()) {
    Serial.println("Good readings");
  } else {
    Serial.println("Calculating...");
  }
  
  // Debug information
  Serial.print("Raw IR: ");
  Serial.print(oximeter.getRawIR());
  Serial.print(" | Raw Red: ");
  Serial.println(oximeter.getRawRed());
  Serial.println();
}