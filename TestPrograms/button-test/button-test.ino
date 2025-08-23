const int BTN = 18;

void setup() {
  pinMode(BTN, INPUT_PULLUP); // internal pull-up resistor
  Serial.begin(115200);
}

void loop() {
  if (digitalRead(BTN) == LOW) {
    Serial.println("Pressed");
  } else {
    Serial.println("Released button");
  }
  delay(1000);
}