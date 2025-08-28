/*
 * ========================================================================================
 * PULSE OXIMETER WRAPPER LIBRARY
 * 
 * This library simplifies the complex MAX30102 sensor operations by providing
 * easy-to-use functions that handle all the technical details internally.
 * 
 * Features handled by this wrapper:
 * - Automatic sensor initialization with optimal settings
 * - Raw data collection and buffering (100-sample rolling window)
 * - SpO₂ and heart rate calculation using Maxim's algorithm
 * - Exponential Moving Average smoothing for stable readings
 * - Automatic LED brightness control (auto-gain) for different finger sizes
 * - Reliable finger detection
 * - Interrupt-driven data collection for efficiency
 * 
 * Usage:
 *   PulseOximeterWrapper oximeter;
 *   oximeter.begin();
 *   oximeter.update();
 *   if (oximeter.isFingerDetected()) {
 *     int spo2 = oximeter.getSpO2();
 *     int hr = oximeter.getHeartRate();
 *   }
 * ========================================================================================
 */

#ifndef PULSE_OXIMETER_WRAPPER_H
#define PULSE_OXIMETER_WRAPPER_H

#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

class PulseOximeterWrapper {
  public:
    /*
     * Constructor - creates the wrapper object
     */
    PulseOximeterWrapper();
    
    /*
     * Initialize the sensor with optimal settings
     * Returns: true if successful, false if sensor not found
     */
    bool begin(TwoWire &wirePort = Wire, uint32_t i2cSpeed = I2C_SPEED_STANDARD);
    
    /*
     * Call this regularly in your main loop to collect data and update calculations
     * This function handles all the complex buffering and algorithm processing
     */
    void update();
    
    /*
     * Check if a finger is detected on the sensor
     * Returns: true if finger is present and giving good signal
     */
    bool isFingerDetected();
    
    /*
     * Get the current SpO₂ reading (blood oxygen saturation)
     * Returns: SpO₂ percentage (95-100% normal), -1 if invalid/no finger
     */
    int getSpO2();
    
    /*
     * Get the current heart rate reading
     * Returns: Heart rate in beats per minute (60-100 normal at rest), -1 if invalid/no finger  
     */
    int getHeartRate();
    
    /*
     * Check if the SpO₂ reading is currently valid and reliable
     * Returns: true if SpO₂ can be trusted, false if unreliable or no finger
     */
    bool isSpO2Valid();
    
    /*
     * Check if the heart rate reading is currently valid and reliable
     * Returns: true if heart rate can be trusted, false if unreliable or no finger
     */
    bool isHeartRateValid();
    
    /*
     * Get the raw infrared sensor reading (for debugging/finger detection)
     * Returns: Raw IR value (>15000 typically indicates finger present)
     */
    uint32_t getRawIR();
    
    /*
     * Get the raw red sensor reading (for debugging)
     * Returns: Raw red value
     */
    uint32_t getRawRed();
    
    /*
     * Enable or disable auto-gain feature
     * Auto-gain automatically adjusts LED brightness for optimal readings
     * Parameters: enabled - true to enable auto-gain, false to use fixed brightness
     */
    void setAutoGain(bool enabled);
    
    /*
     * Reset all internal buffers and calculations
     * Useful when starting a new measurement session
     */
    void reset();
    
    /*
     * Set up interrupt pin for efficient data collection (optional)
     * Parameters: interruptPin - GPIO pin connected to MAX30102 INT output
     */
    void enableInterrupt(int interruptPin);
    
    /*
     * Get status information for debugging
     * Returns: String with current sensor status, buffer state, etc.
     */
    String getStatus();

  private:
    // Hardware objects
    MAX30105 _sensor;
    
    // Configuration constants
    static const byte LED_BRIGHTNESS = 220;
    static const byte SAMPLE_AVG = 8;
    static const byte LED_MODE = 2;
    static const byte SAMPLE_RATE = 100;
    static const int PULSE_WIDTH = 411;
    static const int ADC_RANGE = 16384;
    
    // Data buffers for algorithm
    static const int32_t BUFFER_LENGTH = 100;
    uint32_t _irBuffer[BUFFER_LENGTH];
    uint32_t _redBuffer[BUFFER_LENGTH];
    int _bufferHead;
    bool _bufferReady;
    
    // Calculated values
    int32_t _rawSpO2;
    int8_t _validSpO2;
    int32_t _rawHeartRate;
    int8_t _validHeartRate;
    
    // Smoothed display values
    int _smoothedSpO2;
    int _smoothedHeartRate;
    
    // EMA smoothing parameters
    static const int EMA_NUMERATOR = 3;
    static const int EMA_DENOMINATOR = 10;
    
    // Finger detection
    static const uint32_t FINGER_THRESHOLD = 15000;
    uint32_t _lastIR;
    uint32_t _lastRed;
    
    // Auto-gain control
    bool _autoGainEnabled;
    static const uint32_t IR_TARGET_LOW = 25000;
    static const uint32_t IR_TARGET_HIGH = 90000;
    static const byte LED_MIN = 20;
    static const byte LED_MAX = 255;
    static const byte LED_STEP = 5;
    byte _currentLEDBrightness;
    unsigned long _lastGainAdjustment;
    static const unsigned long GAIN_ADJUSTMENT_INTERVAL = 800;
    
    // Timing
    unsigned long _lastCalculation;
    static const unsigned long CALCULATION_INTERVAL = 1000;
    
    // Private helper functions
    void _collectSensorData();
    void _calculateVitals();
    void _adjustAutoGain();
    int _updateEMA(int currentValue, int newValue);
    void _setupSensorConfiguration();
};

#endif // PULSE_OXIMETER_WRAPPER_H