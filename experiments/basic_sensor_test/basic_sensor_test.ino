/*
 * Basic Sensor Test
 * Just testing if the MAX30102 sensor works and gives readings
 * 
 * This was my first test to see if I could get any data from the sensor
 * If it works you should see numbers changing when you put your finger on it
 */

#include <Wire.h>
#include "MAX30105.h"

MAX30105 sensor;

void setup() {
  Serial.begin(115200);
  Serial.println("Testing MAX30102 sensor...");
  
  // Start I2C
  Wire.begin(21, 22);  // SDA=21, SCL=22 on ESP32
  
  // Try to start sensor
  if (!sensor.begin()) {
    Serial.println("Sensor not found! Check wiring:");
    Serial.println("VIN -> 3.3V, GND -> GND, SDA -> 21, SCL -> 22");
    while(1);  // Stop here
  }
  
  Serial.println("Sensor found!");
  
  // Basic setup - just default settings to see if it works
  sensor.setup();
  
  Serial.println("Put your finger on the sensor...");
  Serial.println("You should see the RED and IR numbers change");
  Serial.println();
}

void loop() {
  // Read the raw values
  long red = sensor.getRed();
  long ir = sensor.getIR();
  
  // Print them out
  Serial.print("RED: ");
  Serial.print(red);
  Serial.print("   IR: ");
  Serial.print(ir);
  
  // Tell user if finger detected
  if (ir > 5000) {
    Serial.println("  <- FINGER DETECTED");
  } else {
    Serial.println("  <- no finger");
  }
  
  delay(500);  // Don't spam the serial monitor
}