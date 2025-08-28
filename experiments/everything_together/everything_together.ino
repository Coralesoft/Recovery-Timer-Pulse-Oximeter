/*
 * Everything Together Test
 * Testing all components working at the same time
 * 
 * This was my test to make sure sensor, display, button and buzzer all work together
 * before I wrote the main program
 */

#include <Wire.h>
#include "MAX30105.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Hardware setup
MAX30105 sensor;
Adafruit_SSD1306 display(128, 32, &Wire, -1);

// Pins
int buttonPin = 25;
int buzzerPin = 15;

// Variables
uint32_t lastIR = 0;
bool buttonPressed = false;
int counter = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Testing everything together...");
  
  // Setup pins
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
  
  // Start I2C
  Wire.begin(21, 22);
  
  // Test display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Display failed!");
    while(1);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("System Test");
  display.println("Starting...");
  display.display();
  delay(2000);
  
  // Test sensor  
  if (!sensor.begin()) {
    Serial.println("Sensor failed!");
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Sensor failed!");
    display.display();
    while(1);
  }
  
  sensor.setup();  // Basic setup
  
  // Test buzzer
  Serial.println("Testing buzzer...");
  digitalWrite(buzzerPin, HIGH);
  delay(200);
  digitalWrite(buzzerPin, LOW);
  
  Serial.println("All systems working! Press button to test");
}

void loop() {
  // Check button
  if (digitalRead(buttonPin) == LOW && !buttonPressed) {
    buttonPressed = true;
    Serial.println("Button pressed!");
    
    // Make beep
    digitalWrite(buzzerPin, HIGH);
    delay(100);
    digitalWrite(buzzerPin, LOW);
    
    counter++;
  }
  
  if (digitalRead(buttonPin) == HIGH) {
    buttonPressed = false;
  }
  
  // Read sensor
  lastIR = sensor.getIR();
  
  // Update display every second
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000) {
    updateDisplay();
    lastUpdate = millis();
  }
  
  delay(10);
}

void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0,0);
  
  display.println("System Test:");
  
  // Show sensor status
  display.print("Sensor: ");
  if (lastIR > 5000) {
    display.println("Finger OK");
  } else {
    display.println("No finger");
  }
  
  // Show button presses
  display.print("Presses: ");
  display.println(counter);
  
  display.display();
}