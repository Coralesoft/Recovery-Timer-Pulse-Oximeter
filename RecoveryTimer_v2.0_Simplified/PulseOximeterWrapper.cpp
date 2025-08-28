/*
 * ========================================================================================
 * PULSE OXIMETER WRAPPER LIBRARY IMPLEMENTATION
 * 
 * This file contains all the complex sensor handling code, allowing the main program
 * to focus on the recovery timer logic rather than sensor technical details.
 * ========================================================================================
 */

#include "PulseOximeterWrapper.h"

// Constructor - initialize all variables to safe defaults
PulseOximeterWrapper::PulseOximeterWrapper() {
  _bufferHead = 0;
  _bufferReady = false;
  _rawSpO2 = -1;
  _validSpO2 = 0;
  _rawHeartRate = -1;
  _validHeartRate = 0;
  _smoothedSpO2 = -1;
  _smoothedHeartRate = -1;
  _lastIR = 0;
  _lastRed = 0;
  _autoGainEnabled = true;
  _currentLEDBrightness = LED_BRIGHTNESS;
  _lastGainAdjustment = 0;
  _lastCalculation = 0;
}

/*
 * Initialize the MAX30102 sensor with optimal settings for SpO2 measurement
 */
bool PulseOximeterWrapper::begin(TwoWire &wirePort, uint32_t i2cSpeed) {
  // Try to initialize the sensor
  if (!_sensor.begin(wirePort, i2cSpeed)) {
    return false; // Sensor not found
  }
  
  // Configure sensor with optimal settings
  _setupSensorConfiguration();
  
  // Reset all internal state
  reset();
  
  return true;
}

/*
 * Main update function - call this regularly in your main loop
 * Handles all data collection, processing, and calculations
 */
void PulseOximeterWrapper::update() {
  // Collect new sensor data
  _collectSensorData();
  
  // Calculate SpO2 and heart rate if enough data is available
  if (_bufferReady && millis() - _lastCalculation >= CALCULATION_INTERVAL) {
    _calculateVitals();
    _lastCalculation = millis();
  }
  
  // Adjust LED brightness for optimal signal quality
  if (_autoGainEnabled) {
    _adjustAutoGain();
  }
  
  // Keep sensor communication alive
  _sensor.check();
}

/*
 * Check if a finger is detected on the sensor
 */
bool PulseOximeterWrapper::isFingerDetected() {
  return (_lastIR >= FINGER_THRESHOLD);
}

/*
 * Get smoothed SpO2 reading
 */
int PulseOximeterWrapper::getSpO2() {
  if (!isFingerDetected() || !_validSpO2 || _smoothedSpO2 < 0) {
    return -1; // Invalid reading
  }
  return _smoothedSpO2;
}

/*
 * Get smoothed heart rate reading
 */
int PulseOximeterWrapper::getHeartRate() {
  if (!isFingerDetected() || !_validHeartRate || _smoothedHeartRate < 0) {
    return -1; // Invalid reading
  }
  return _smoothedHeartRate;
}

/*
 * Check if SpO2 reading is valid
 */
bool PulseOximeterWrapper::isSpO2Valid() {
  return (isFingerDetected() && _validSpO2 && _smoothedSpO2 > 0);
}

/*
 * Check if heart rate reading is valid
 */
bool PulseOximeterWrapper::isHeartRateValid() {
  return (isFingerDetected() && _validHeartRate && _smoothedHeartRate > 0);
}

/*
 * Get raw IR sensor value
 */
uint32_t PulseOximeterWrapper::getRawIR() {
  return _lastIR;
}

/*
 * Get raw red sensor value
 */
uint32_t PulseOximeterWrapper::getRawRed() {
  return _lastRed;
}

/*
 * Enable or disable automatic gain control
 */
void PulseOximeterWrapper::setAutoGain(bool enabled) {
  _autoGainEnabled = enabled;
}

/*
 * Reset all buffers and calculations - useful for starting fresh measurement
 */
void PulseOximeterWrapper::reset() {
  _bufferHead = 0;
  _bufferReady = false;
  _smoothedSpO2 = -1;
  _smoothedHeartRate = -1;
  _validSpO2 = 0;
  _validHeartRate = 0;
  
  // Clear data buffers
  for (int i = 0; i < BUFFER_LENGTH; i++) {
    _irBuffer[i] = 0;
    _redBuffer[i] = 0;
  }
}

/*
 * Get status information for debugging
 */
String PulseOximeterWrapper::getStatus() {
  String status = "PulseOximeterWrapper Status:\n";
  status += "Finger: " + String(isFingerDetected() ? "Yes" : "No") + "\n";
  status += "Buffer Ready: " + String(_bufferReady ? "Yes" : "No") + "\n";
  status += "SpO2 Valid: " + String(_validSpO2 ? "Yes" : "No") + "\n";
  status += "HR Valid: " + String(_validHeartRate ? "Yes" : "No") + "\n";
  status += "Raw IR: " + String(_lastIR) + "\n";
  status += "LED Brightness: " + String(_currentLEDBrightness) + "\n";
  status += "Auto-gain: " + String(_autoGainEnabled ? "On" : "Off") + "\n";
  return status;
}

// ===== PRIVATE HELPER FUNCTIONS =====

/*
 * Configure the MAX30102 sensor with optimal settings
 */
void PulseOximeterWrapper::_setupSensorConfiguration() {
  _sensor.setup(LED_BRIGHTNESS, SAMPLE_AVG, LED_MODE, SAMPLE_RATE, PULSE_WIDTH, ADC_RANGE);
  _sensor.setPulseAmplitudeRed(LED_BRIGHTNESS);
  _sensor.setPulseAmplitudeIR(LED_BRIGHTNESS);
  _sensor.setPulseAmplitudeGreen(0); // Turn off green LED
  _currentLEDBrightness = LED_BRIGHTNESS;
}

/*
 * Collect raw sensor data and store in circular buffer
 */
void PulseOximeterWrapper::_collectSensorData() {
  // Read all available samples from sensor FIFO
  while (_sensor.available()) {
    uint32_t red = _sensor.getRed();
    uint32_t ir = _sensor.getIR();
    _sensor.nextSample();
    
    // Store most recent values for finger detection and auto-gain
    _lastRed = red;
    _lastIR = ir;
    
    // Add to circular buffer
    _redBuffer[_bufferHead] = red;
    _irBuffer[_bufferHead] = ir;
    _bufferHead = (_bufferHead + 1) % BUFFER_LENGTH;
    
    // Mark buffer as ready when we've filled it once
    if (!_bufferReady && _bufferHead == 0) {
      _bufferReady = true;
    }
  }
}

/*
 * Calculate SpO2 and heart rate using Maxim's algorithm, then apply smoothing
 */
void PulseOximeterWrapper::_calculateVitals() {
  // Only calculate if finger is detected
  if (!isFingerDetected()) {
    _validSpO2 = 0;
    _validHeartRate = 0;
    return;
  }
  
  // Run Maxim's SpO2 and heart rate algorithm
  maxim_heart_rate_and_oxygen_saturation(_irBuffer, BUFFER_LENGTH, _redBuffer, 
                                         &_rawSpO2, &_validSpO2, &_rawHeartRate, &_validHeartRate);
  
  // Apply smoothing to valid readings
  if (_validSpO2 && _rawSpO2 > 0 && _rawSpO2 <= 100) {
    _smoothedSpO2 = _updateEMA(_smoothedSpO2, (int)_rawSpO2);
  }
  
  if (_validHeartRate && _rawHeartRate > 0 && _rawHeartRate < 240) {
    _smoothedHeartRate = _updateEMA(_smoothedHeartRate, (int)_rawHeartRate);
  }
}

/*
 * Automatically adjust LED brightness for optimal signal strength
 */
void PulseOximeterWrapper::_adjustAutoGain() {
  unsigned long now = millis();
  
  // Don't adjust too frequently
  if (now - _lastGainAdjustment < GAIN_ADJUSTMENT_INTERVAL) {
    return;
  }
  
  // Only adjust if finger is detected
  if (!isFingerDetected()) {
    return;
  }
  
  bool changed = false;
  
  // Increase brightness if signal is too weak
  if (_lastIR < IR_TARGET_LOW && _currentLEDBrightness < LED_MAX) {
    byte newBrightness = _currentLEDBrightness + LED_STEP;
    _currentLEDBrightness = (newBrightness > LED_MAX) ? LED_MAX : newBrightness;
    changed = true;
  }
  // Decrease brightness if signal is too strong  
  else if (_lastIR > IR_TARGET_HIGH && _currentLEDBrightness > LED_MIN) {
    int newBrightness = _currentLEDBrightness - LED_STEP;
    _currentLEDBrightness = (newBrightness < LED_MIN) ? LED_MIN : (byte)newBrightness;
    changed = true;
  }
  
  // Apply new brightness setting
  if (changed) {
    _sensor.setPulseAmplitudeRed(_currentLEDBrightness);
    _sensor.setPulseAmplitudeIR(_currentLEDBrightness);
    _lastGainAdjustment = now;
  }
}

/*
 * Update Exponential Moving Average for smoothing noisy readings
 * Formula: new_average = (old_average * 0.7) + (new_reading * 0.3)
 */
int PulseOximeterWrapper::_updateEMA(int currentValue, int newValue) {
  if (currentValue < 0) {
    return newValue; // First reading - no previous average
  }
  return ((EMA_DENOMINATOR - EMA_NUMERATOR) * currentValue + EMA_NUMERATOR * newValue) / EMA_DENOMINATOR;
}