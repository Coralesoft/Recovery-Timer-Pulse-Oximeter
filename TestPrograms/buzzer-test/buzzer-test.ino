const int BUZZER = 19;

void setup() {
  pinMode(BUZZER, OUTPUT);
}

void loop() {
  // Simple beep
  digitalWrite(BUZZER, HIGH);
  delay(200);
  digitalWrite(BUZZER, LOW);
  delay(800);
}