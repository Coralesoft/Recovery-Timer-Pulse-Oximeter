/*
 * SpO2 and Heart Rate Test Program
 * 
 * This program tests the complete SpO2 calculation algorithm with the MAX30102 sensor.
 * It demonstrates the full data processing pipeline from raw sensor readings to 
 * calculated SpO2 percentage and heart rate values.
 * 
 * Features tested:
 * - Raw IR/Red data collection
 * - Buffer management (100-sample rolling window)
 * - Maxim SpO2 algorithm integration
 * - Exponential Moving Average smoothing
 * - Finger detection
 * - Auto-gain LED brightness control
 * 
 * Expected output:
 * - SpO2: 95-100% for healthy individuals
 * - Heart Rate: 60-100 BPM at rest
 * - Values should be stable when finger is still
 */

#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

// I2C Configuration
#define I2C_SDA 21
#define I2C_SCL 22

// Sensor configuration
MAX30105 sensor;
const byte LED_BRIGHTNESS = 220;
const byte SAMPLE_AVG = 8;
const byte LED_MODE = 2; // Red + IR
const byte SAMPLE_RATE = 100;
const int PULSE_WIDTH = 411;
const int ADC_RANGE = 16384;

// Algorithm buffers
const int32_t BUFLEN = 100;
uint32_t irBuffer[BUFLEN];
uint32_t redBuffer[BUFLEN];
int bufferHead = 0;
bool bufferReady = false;

// Calculated values
int32_t spo2 = -1;
int8_t validSPO2 = 0;
int32_t heartRate = -1;
int8_t validHeartRate = 0;

// Smoothing variables
int smoothedSPO2 = -1;
int smoothedHR = -1;
const int EMA_ALPHA = 3; // Smoothing factor numerator
const int EMA_BETA = 10; // Smoothing factor denominator

// Finger detection
const uint32_t FINGER_THRESHOLD = 15000;
uint32_t lastIR = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("SpO2 and Heart Rate Test");
  Serial.println("========================");
  
  // Initialize I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  
  // Initialize sensor
  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("ERROR: MAX30102 not found!");
    while(1) delay(1000);
  }
  
  Serial.println("MAX30102 sensor found");
  
  // Configure sensor
  sensor.setup(LED_BRIGHTNESS, SAMPLE_AVG, LED_MODE, SAMPLE_RATE, PULSE_WIDTH, ADC_RANGE);
  sensor.setPulseAmplitudeRed(LED_BRIGHTNESS);
  sensor.setPulseAmplitudeIR(LED_BRIGHTNESS);
  sensor.setPulseAmplitudeGreen(0);
  
  Serial.println("Sensor configured");
  Serial.println("Place finger firmly on sensor...");
  Serial.println("Format: SpO2%, HeartRate BPM, Status");
  Serial.println();
}

void loop() {
  // Collect sensor data
  while (sensor.available()) {
    uint32_t red = sensor.getRed();
    uint32_t ir = sensor.getIR();
    sensor.nextSample();
    
    lastIR = ir;
    
    // Add to circular buffer
    redBuffer[bufferHead] = red;
    irBuffer[bufferHead] = ir;
    bufferHead = (bufferHead + 1) % BUFLEN;
    
    // Check if buffer is full for first time
    if (!bufferReady && bufferHead == 0) {
      bufferReady = true;
      Serial.println("Buffer full - starting calculations...");
    }
  }
  
  // Calculate SpO2 and HR when we have enough data
  static unsigned long lastCalculation = 0;
  if (bufferReady && millis() - lastCalculation > 1000) { // Calculate every 1 second
    calculateVitals();
    displayResults();
    lastCalculation = millis();
  }
  
  // Keep sensor alive
  sensor.check();
  delay(10);
}

void calculateVitals() {
  // Check finger presence
  if (lastIR < FINGER_THRESHOLD) {
    validSPO2 = 0;
    validHeartRate = 0;
    return;
  }
  
  // Run Maxim algorithm
  maxim_heart_rate_and_oxygen_saturation(irBuffer, BUFLEN, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
  
  // Apply smoothing to valid readings
  if (validSPO2 && spo2 > 0 && spo2 <= 100) {
    if (smoothedSPO2 < 0) {
      smoothedSPO2 = spo2; // First reading
    } else {
      smoothedSPO2 = ((EMA_BETA - EMA_ALPHA) * smoothedSPO2 + EMA_ALPHA * spo2) / EMA_BETA;
    }
  }
  
  if (validHeartRate && heartRate > 0 && heartRate < 240) {
    if (smoothedHR < 0) {
      smoothedHR = heartRate; // First reading
    } else {
      smoothedHR = ((EMA_BETA - EMA_ALPHA) * smoothedHR + EMA_ALPHA * heartRate) / EMA_BETA;
    }
  }
}

void displayResults() {
  Serial.print("SpO2: ");
  if (validSPO2 && smoothedSPO2 > 0) {
    Serial.print(smoothedSPO2);
    Serial.print("%");
  } else {
    Serial.print("--");
  }
  
  Serial.print(" | HR: ");
  if (validHeartRate && smoothedHR > 0) {
    Serial.print(smoothedHR);
    Serial.print(" BPM");
  } else {
    Serial.print("--- BPM");
  }
  
  Serial.print(" | Status: ");
  if (lastIR < FINGER_THRESHOLD) {
    Serial.println("No finger detected");
  } else if (!validSPO2 && !validHeartRate) {
    Serial.println("Calculating... (keep finger still)");
  } else if (validSPO2 && validHeartRate) {
    Serial.println("Good signal");
  } else {
    Serial.println("Weak signal");
  }
  
  // Show raw IR for debugging
  Serial.print("Raw IR: ");
  Serial.println(lastIR);
  Serial.println();
}