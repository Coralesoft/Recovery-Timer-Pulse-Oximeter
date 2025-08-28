/*
 * Buzzer Test
 * Testing the buzzer to make sure it makes sound
 * 
 * Simple test that makes different beep patterns
 * You should hear beeps if the buzzer is connected right
 */

int buzzerPin = 15;  // Buzzer connected to pin 15

void setup() {
  Serial.begin(115200);
  Serial.println("Testing buzzer on pin 15...");
  
  pinMode(buzzerPin, OUTPUT);      // Set pin as output
  digitalWrite(buzzerPin, LOW);    // Make sure it starts off
  
  Serial.println("You should hear different beep patterns");
  delay(1000);
  
  // Test different beep patterns
  Serial.println("Single beep...");
  singleBeep();
  delay(1000);
  
  Serial.println("Double beep...");
  doubleBeep();
  delay(1000);
  
  Serial.println("Triple beep...");
  tripleBeep();
  delay(1000);
  
  Serial.println("Long beep...");
  longBeep();
  delay(1000);
  
  Serial.println("Buzzer test complete!");
}

void loop() {
  // Just beep once every 5 seconds to show it's working
  Serial.println("Beep!");
  singleBeep();
  delay(5000);
}

// Different beep functions I can use in my main program
void singleBeep() {
  digitalWrite(buzzerPin, HIGH);  // Turn on
  delay(100);                     // Wait
  digitalWrite(buzzerPin, LOW);   // Turn off
}

void doubleBeep() {
  singleBeep();
  delay(100);
  singleBeep();
}

void tripleBeep() {
  singleBeep();
  delay(100);
  singleBeep();
  delay(100);
  singleBeep();
}

void longBeep() {
  digitalWrite(buzzerPin, HIGH);
  delay(500);                    // Longer beep
  digitalWrite(buzzerPin, LOW);
}