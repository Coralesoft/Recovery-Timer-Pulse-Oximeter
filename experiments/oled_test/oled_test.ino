/*
 * OLED Display Test
 * Testing the small OLED screen
 * 
 * This test shows different things on the OLED to make sure it works
 * If successful you should see text and numbers on the display
 */

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1

// Create display object (most OLEDs use address 0x3C)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);
  Serial.println("Testing OLED display...");
  
  // Start I2C
  Wire.begin(21, 22);  // SDA=21, SCL=22
  
  // Try to start display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found! Check wiring:");
    Serial.println("VCC -> 3.3V, GND -> GND, SDA -> 21, SCL -> 22");
    while(1);
  }
  
  Serial.println("OLED found and working!");
  
  // Clear the display
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);  // White text
  display.setTextSize(1);               // Normal size text
  display.setCursor(0, 0);              // Top left corner
  
  // Show some text
  display.println("OLED Test Working!");
  display.println("Made by Max Brown");
  display.println("Year 12 Electronics");
  
  display.display();  // Actually show it on screen
  
  delay(3000);  // Show for 3 seconds
}

void loop() {
  // Show a counter that updates every second
  static int counter = 0;
  
  display.clearDisplay();
  display.setCursor(0, 0);
  
  display.setTextSize(2);  // Bigger text
  display.println("Counter:");
  display.print(counter);
  
  display.display();  // Update the screen
  
  counter++;
  if (counter > 999) counter = 0;  // Reset at 1000
  
  delay(1000);  // Update every second
}
