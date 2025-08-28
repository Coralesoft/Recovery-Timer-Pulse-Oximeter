/*
 * Button Test
 * Testing if my button works properly
 * 
 * Just a simple test to make sure the button is wired right
 * Should print messages when you press and release it
 */

// Button connected to pin 25
int buttonPin = 25;

void setup() {
  Serial.begin(115200);
  Serial.println("Testing button on pin 25...");
  
  // Set pin as input with pullup resistor
  pinMode(buttonPin, INPUT_PULLUP);
  
  Serial.println("Press the button and you should see messages");
}

void loop() {
  // Read the button (LOW = pressed because of pullup resistor)
  int buttonState = digitalRead(buttonPin);
  
  // Keep track of previous state to detect changes
  static int lastState = HIGH;
  
  // Button just got pressed
  if (buttonState == LOW && lastState == HIGH) {
    Serial.println("BUTTON PRESSED!");
  }
  
  // Button just got released  
  if (buttonState == HIGH && lastState == LOW) {
    Serial.println("Button released");
  }
  
  lastState = buttonState;
  
  delay(50);  // Small delay for debouncing
}