/*
 * Heart Rate Test
 * Testing if I can get actual heart rate readings (not just raw data)
 * 
 * This uses the proper algorithm to calculate heart rate from the sensor
 * It's more complex than the basic sensor test but needed for my project
 */

#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"  // Algorithm for calculating heart rate and oxygen

MAX30105 sensor;

// Buffer to store readings for the algorithm
#define BUFFER_SIZE 100
uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];
int bufferIndex = 0;
bool bufferFull = false;

void setup() {
  Serial.begin(115200);
  Serial.println("Heart Rate Algorithm Test");
  Serial.println("This might take a minute to get stable readings...");
  
  Wire.begin(21, 22);
  
  if (!sensor.begin()) {
    Serial.println("Sensor not found!");
    while(1);
  }
  
  Serial.println("Sensor found!");
  
  // Setup sensor with good settings for heart rate
  byte ledBrightness = 200;  // LED brightness
  byte sampleAverage = 8;    // Average samples
  byte ledMode = 2;         // Red + IR mode  
  byte sampleRate = 100;    // 100 samples per second
  int pulseWidth = 411;     // Pulse width
  int adcRange = 16384;     // ADC range
  
  sensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  
  Serial.println("Put finger on sensor and keep it very still...");
  Serial.println("Format: HeartRate BPM, SpO2%, Status");
}

void loop() {
  // Read sensor data
  if (sensor.available()) {
    uint32_t red = sensor.getRed();
    uint32_t ir = sensor.getIR();
    sensor.nextSample();
    
    // Store in buffer
    redBuffer[bufferIndex] = red;
    irBuffer[bufferIndex] = ir;
    bufferIndex++;
    
    // When buffer is full, calculate heart rate
    if (bufferIndex >= BUFFER_SIZE) {
      bufferIndex = 0;
      bufferFull = true;
      
      // Only calculate if finger is detected
      if (ir > 7000) {  // Finger detection threshold
        calculateHeartRate();
      } else {
        Serial.println("No finger detected - place finger firmly on sensor");
      }
    }
  }
  
  sensor.check();  // Keep sensor alive
}

void calculateHeartRate() {
  // Variables for algorithm results
  int32_t spo2;       // Oxygen saturation  
  int8_t validSPO2;   // 1 if valid, 0 if not
  int32_t heartRate;  // Heart rate
  int8_t validHR;     // 1 if valid, 0 if not
  
  // Run the algorithm (this is from Maxim, the sensor company)
  maxim_heart_rate_and_oxygen_saturation(irBuffer, BUFFER_SIZE, redBuffer, 
                                         &spo2, &validSPO2, &heartRate, &validHR);
  
  // Print results
  Serial.print("HR: ");
  if (validHR && heartRate > 0) {
    Serial.print(heartRate);
    Serial.print(" bpm");
  } else {
    Serial.print("calculating...");
  }
  
  Serial.print("  |  SpO2: ");
  if (validSPO2 && spo2 > 0) {
    Serial.print(spo2);
    Serial.print("%");
  } else {
    Serial.print("calculating...");
  }
  
  Serial.print("  |  ");
  if (validHR && validSPO2) {
    Serial.println("Good readings!");
  } else {
    Serial.println("Keep finger still...");
  }
}