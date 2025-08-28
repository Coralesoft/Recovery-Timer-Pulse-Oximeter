# Test Programs Documentation
## Advanced Testing Suite for Recovery Timer Project

---

## Overview
This folder contains the advanced test programs I developed while working on my recovery timer. These are more sophisticated than the basic tests in `/experiments/` and show how my testing evolved as I learned more about the components.

## Why Two Testing Folders?

### /experiments/ folder
- **Purpose**: Basic "hello world" style tests
- **When created**: Week 1-2 when first learning components
- **Complexity**: Simple, single-purpose tests
- **Goal**: Just make sure each part works

### /TestPrograms/ folder (this one)
- **Purpose**: Advanced testing and debugging
- **When created**: Week 3-6 during integration and optimization  
- **Complexity**: More sophisticated, multiple features
- **Goal**: Solve specific problems and test advanced features

Think of it like learning to drive - `/experiments/` is the parking lot practice, `/TestPrograms/` is actual road driving!

---

## Test Program Descriptions

### 📡 i2c-scanner/
**Purpose**: Find I2C addresses of connected devices  
**Why needed**: My OLED wouldn't work - turned out it was 0x3C not 0x27!  
**Key learning**: Always scan for actual addresses, don't trust documentation blindly

### max30102-raw/
**Purpose**: Read raw sensor values without any processing  
**Complexity**: More advanced than basic_sensor_test  
**What it shows**: Raw RED and IR values, helps understand sensor behavior  
**Key discovery**: IR values range from 1000 (no finger) to 15000+ (finger present)

### lcd-tests/
**Purpose**: Test LCD display before switching to OLED  
**Why important**: Shows my iteration from LCD → OLED  
**Result**: LCD worked but was too big and power-hungry

###  oled-test1/
**Purpose**: Advanced OLED testing with graphics and animations  
**Difference from experiments/oled_test**: This one tests graphics, not just text  
**Used for**: Designing the UI layout before implementing in main code

### oled-mock-ui/
**Purpose**: Mock-up the actual user interface without sensor  
**Why created**: Faster to test UI changes without waiting for sensor readings  
**Key feature**: Simulates state changes with button presses

### spo2-test/
**Purpose**: Test the SpO2 algorithm in isolation  
**Complexity**: Uses Maxim's actual algorithm  
**Challenge**: Understanding the 100-sample buffer requirement  
**Result**: Learned why readings take 30+ seconds to stabilize

### buzzer-test/
**Purpose**: Test different beep patterns and tones  
**Advanced features**: Multiple beep patterns, not just on/off  
**Used for**: Designing audio feedback system (short beep, long beep, error beep)

### button-test/
**Purpose**: Advanced button handling with debouncing  
**Key feature**: Detects short vs long press  
**Problem solved**: Eliminated multiple press detection issues

### system-integration/
**Purpose**: Test all components working together  
**Why needed**: Check for conflicts (shared I2C bus, power issues)  
**Key discovery**: No conflicts! I2C sharing works perfectly

### working-oled-web/
**Purpose**: Test OLED with web server for remote monitoring  
**Status**: Experimental - maybe for version 2?  
**Cool feature**: Shows readings on phone browser

### working-oled-noweb/
**Purpose**: Same as above but without WiFi  
**Why both**: Testing if WiFi affects sensor readings (it doesn't!)

### wrapper-test/
**Purpose**: Test the sensor wrapper library I tried to create  
**Result**: Decided against it - made code more complex, not simpler  
**Learning**: Sometimes "better" organization isn't actually better!

---

## Testing Evolution Timeline

### Week 1-2: Basic Testing
Started with `/experiments/` - just making LEDs blink and sensors respond

### Week 3: Problem Solving
Created `i2c-scanner` when OLED wouldn't work  
Made `max30102-raw` to understand sensor behavior

### Week 4: Algorithm Testing
Developed `spo2-test` to isolate algorithm issues  
Used `oled-mock-ui` to design interface without sensor delays

### Week 5: Integration Testing
Built `system-integration` to verify everything works together  
Created advanced `button-test` for long-press detection

### Week 6: Future Features
Experimented with `working-oled-web` for WiFi features  
Tried `wrapper-test` but decided to keep code simple

---

## Key Discoveries from Testing

1. **I2C Address Issues** (i2c-scanner)
   - Never trust the silk screen labels!
   - Always scan for actual addresses

2. **Sensor Behavior** (max30102-raw)
   - Finger detection threshold needs to be >6500
   - Takes time for readings to stabilize

3. **Display Choice** (lcd-tests vs oled-tests)
   - OLED uses less power and is clearer
   - 128x32 pixels is enough for essential info

4. **Algorithm Requirements** (spo2-test)
   - Needs 100 continuous samples
   - Can't just take single readings

5. **UI Design** (oled-mock-ui)
   - Test interface separately from sensors
   - Much faster development cycle

---

## How These Tests Improved the Final Product

- **i2c-scanner** → Saved hours of debugging, found correct addresses
- **max30102-raw** → Led to optimal threshold of 6500 for finger detection  
- **spo2-test** → Understood why 100-sample buffer is essential
- **button-test** → Implemented debouncing and long-press in final code
- **system-integration** → Confirmed no component conflicts before final assembly
- **oled-mock-ui** → Refined UI without waiting for sensor readings

---

## Running These Tests

Each test is standalone - just upload to ESP32 and open Serial Monitor at 115200 baud.

**Required libraries**: Same as main project
- SparkFun MAX3010x
- Adafruit SSD1306
- Adafruit GFX

**Wiring**: Use same connections as final project (see main README)

---

## Lessons Learned

1. **Test everything in isolation first** - Much easier to debug
2. **Keep your test programs** - Useful for future debugging  
3. **Document your discoveries** - I forgot what some early tests proved!
4. **Advanced isn't always better** - wrapper-test proved this
5. **Mock-ups save time** - oled-mock-ui sped up UI development significantly

---

*These test programs represent about 40% of my total development time. Testing isn't just debugging - it's learning how things actually work versus how you think they work!*
