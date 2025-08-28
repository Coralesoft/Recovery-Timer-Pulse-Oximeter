/*
 * I2C Device Scanner
 * 
 * This program scans the I2C bus and reports all detected devices.
 * Useful for troubleshooting I2C connections and confirming device addresses.
 * 
 * Expected devices for this project:
 * - 0x3C: SSD1306 OLED Display
 * - 0x57: MAX30102 Pulse Oximeter Sensor
 * 
 * If devices are not detected, check:
 * - Power connections (3V3, GND)
 * - I2C connections (SDA=21, SCL=22 for ESP32)
 * - Pull-up resistors (usually built-in on modules)
 */

#include <Wire.h>

// I2C pin configuration for ESP32
#define I2C_SDA 21
#define I2C_SCL 22

void setup() {
  Serial.begin(115200);
  Serial.println("I2C Device Scanner");
  Serial.println("==================");
  
  // Initialize I2C with custom pins
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000); // Standard 100kHz for scanning
  
  Serial.print("Scanning I2C bus (SDA=");
  Serial.print(I2C_SDA);
  Serial.print(", SCL=");
  Serial.print(I2C_SCL);
  Serial.println(")...");
  Serial.println();
}

void loop() {
  int deviceCount = 0;
  
  Serial.println("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
  
  for (int row = 0; row < 8; row++) {
    Serial.print(row);
    Serial.print("0: ");
    
    for (int col = 0; col < 16; col++) {
      int address = (row * 16) + col;
      
      if (address < 8 || address > 119) {
        Serial.print("   ");
        continue;
      }
      
      Wire.beginTransmission(address);
      int error = Wire.endTransmission();
      
      if (error == 0) {
        Serial.print("0x");
        if (address < 16) Serial.print("0");
        Serial.print(address, HEX);
        deviceCount++;
        
        // Add device identification
        switch(address) {
          case 0x3C:
            Serial.print("(OLED)");
            break;
          case 0x57:
            Serial.print("(MAX30102)");
            break;
          case 0x27:
            Serial.print("(LCD)");
            break;
          default:
            Serial.print("     ");
        }
      } else {
        Serial.print("-- ");
      }
    }
    Serial.println();
  }
  
  Serial.println();
  Serial.print("Found ");
  Serial.print(deviceCount);
  Serial.println(" device(s)");
  
  if (deviceCount == 0) {
    Serial.println("No I2C devices found. Check:");
    Serial.println("- Power connections (3V3, GND)");
    Serial.println("- I2C wiring (SDA=21, SCL=22)");
    Serial.println("- Device power-on status");
  }
  
  Serial.println("\nScanning again in 5 seconds...");
  Serial.println("========================================");
  delay(5000);
}