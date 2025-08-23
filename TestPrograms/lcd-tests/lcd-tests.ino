// Core I²C library used by the ESP32
#include <Wire.h>
// LCD library for I²C 16x2 screens
#include <LiquidCrystal_I2C.h>

// Create an LCD object at I²C address 0x27, 16 columns, 2 rows
// ( I²C scanner reports its 0x27)
LiquidCrystal_I2C lcd(0x27, 16, 2);
// default I2C pins are SDA=21, SCL=22. Explicitly set them:
const int SDA_PIN = 21;
const int SCL_PIN = 22;
void setup() {
   Wire.begin(SDA_PIN, SCL_PIN);
  // Initialises the LCD controller (must be called before printing)
  lcd.init();
  // Turns on the LCD backlight (so text is visible)
  lcd.backlight();
  // Puts the cursor at column 0, row 0 (top-left corner)
  lcd.setCursor(0, 0);
  // Prints the project title on the first line
  lcd.print("Max's Oxy Meter");
  // Moves the cursor to column 0, row 1 (second line)
  lcd.setCursor(0, 1);
  // Prints a simple version tag on the second line
  lcd.print("Version:1.0");
}

void loop() {
  // Position the cursor at the start of the second line
  lcd.setCursor(0, 1);
  // Waits 2 seconds so the first screen is readable
  delay(2000);
  // Clears the whole display (both lines)
  lcd.clear();
  // Prints a status message on the first line
  lcd.print("Reading Finger");
  // Moves to the second line
  lcd.setCursor(0, 1);
  // Prints placeholders for oxygen saturation and pulse rate
  lcd.print("Oxy:   Pulse: ");
  // Holds this screen for 1 second before looping again
  delay(1000);
}
