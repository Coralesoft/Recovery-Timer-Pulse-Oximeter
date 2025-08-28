# How to Build the Recovery Timer

## What you need
- ESP32 development board
- MAX30102 heart rate sensor module  
- 0.91" OLED display (128x32, I2C)
- Push button
- Small buzzer
- Breadboard and jumper wires
- USB cable for programming

## Step 1: Wire everything up
```
MAX30102 Sensor:
VIN → ESP32 3.3V
GND → ESP32 GND
SDA → ESP32 GPIO21
SCL → ESP32 GPIO22

OLED Display:  
VCC → ESP32 3.3V
GND → ESP32 GND  
SDA → ESP32 GPIO21 (same wire as sensor)
SCL → ESP32 GPIO22 (same wire as sensor)

Push Button:
One side → ESP32 GPIO25
Other side → ESP32 GND
(The code uses INPUT_PULLUP so no external resistor needed)

Buzzer:
Positive → ESP32 GPIO15
Negative → ESP32 GND
```

## Step 2: Install libraries
In Arduino IDE go to Tools > Manage Libraries and install:
- "Adafruit SSD1306" by Adafruit
- "Adafruit GFX Library" by Adafruit  
- "SparkFun MAX3010x library" by SparkFun Electronics

## Step 3: Test components
Before uploading the main program, test each part:
1. Upload `experiments/basic_sensor_test/` - check sensor works
2. Upload `experiments/button_test/` - check button works  
3. Upload `experiments/oled_test/` - check display works
4. Upload `experiments/buzzer_test/` - check buzzer works
5. Upload `experiments/everything_together/` - check all parts work together

## Step 4: Upload main program
Upload `RecoveryTimer/RecoveryTimer.ino` to your ESP32

## Step 5: Test it
1. Open Serial Monitor (115200 baud) to see debug messages
2. Place finger firmly on sensor 
3. Press button to start baseline capture
4. Follow instructions on display

## Troubleshooting
**"Sensor not found"** - Check wiring, make sure VIN goes to 3.3V not 5V  
**"Display not found"** - Try I2C scanner, most displays use 0x3C address  
**Readings jump around** - Keep finger very still, don't press too hard  
**Button doesn't work** - Check it's wired between GPIO25 and GND  
**No sound** - Check buzzer polarity, make sure it's an active buzzer

## Tips
- Use a breadboard first, then solder if you want permanent version
- Keep wires short to avoid interference  
- The sensor needs good finger contact - clean sensor surface if readings are bad
- Takes about 10-20 seconds for heart rate readings to stabilize

## Photos
Check the `photos/` folder for pictures of my circuit setup.