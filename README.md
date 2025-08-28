# Recovery Timer Project
**Year 12 Electronics - Wellington College 2025**  
**Made by: Max Brown**

## What it does
This project measures how fast your heart rate and oxygen levels go back to normal after doing exercise. It's like a fitness tracker but I made it myself using an ESP32 and some sensors.

## How it works
1. Put your finger on the sensor and press the button
2. It takes your normal heart rate and oxygen level for 10 seconds  
3. Take your finger off and do some exercise (jumping jacks, run upstairs etc)
4. Put your finger back on - it starts timing automatically
5. When your levels get back to normal it stops and shows your recovery time
6. It saves your results so you can track your progress

## Parts I used
- ESP32 development board (the main computer)
- MAX30102 sensor (measures heart rate and oxygen in your blood)
- Small OLED screen to show the readings
- Push button to control it
- Buzzer for sound feedback
- Jumper wires and breadboard

## How to use it
1. Connect everything according to the wiring diagram (see photos folder)
2. Upload the code to your ESP32 using Arduino IDE
3. Open serial monitor to see debug info
4. Place finger on sensor and press button to start
5. Follow the instructions on the screen

## Wiring connections
```
MAX30102 Sensor:
VIN → ESP32 3.3V
GND → ESP32 GND
SDA → ESP32 GPIO21  
SCL → ESP32 GPIO22

OLED Display:
VCC → ESP32 3.3V
GND → ESP32 GND
SDA → ESP32 GPIO21 (same as sensor)
SCL → ESP32 GPIO22 (same as sensor)

Button → ESP32 GPIO25 and GND (no external resistor needed)
Buzzer → ESP32 GPIO15
```

## Libraries needed
You need to install these in Arduino IDE:
- Adafruit SSD1306 (for the OLED)
- Adafruit GFX Library
- SparkFun MAX3010x library

## Problems I had and how I fixed them
- **Readings were really jumpy** - Added simple averaging to smooth them out
- **Finger detection not working** - Had to adjust the threshold value from 5000 to 6500
- **Display kept flickering** - Only update it every 500ms instead of every loop
- **Sometimes it wouldn't detect exercise** - Lowered the heart rate increase needed from 15% to 8%
- **OLED wouldn't work at first** - Turns out I had the wrong I2C address, changed to 0x3C

## How I tested it
I made separate test programs for each part:
- Button test - just prints when pressed
- Sensor test - shows raw readings  
- Display test - shows different text
- Buzzer test - makes beeping sounds

## Future improvements
- Maybe add WiFi to send data to my phone
- Add a web interface to view history
- Make the readings more stable
- Add different exercise types

## Photos
Check the `photos/` folder for pictures of my circuit setup.

## Assessment requirements met
✅ Timer-based project  
✅ Captures and stores data  
✅ User can control it repeatedly  
✅ Uses advanced programming (interrupts, algorithms, data structures)  
✅ Has subsystems (sensor, display, input, storage)  
✅ Shows iterative development through testing  

## Credits
- Got help from SparkFun tutorials for the sensor
- OLED display code based on Adafruit examples
- Heart rate algorithm from Maxim (sensor manufacturer)
- Thanks to [teacher name] for help with debugging

---
*This project took me about 3 weeks to build and debug. The hardest part was getting stable readings from the sensor!*