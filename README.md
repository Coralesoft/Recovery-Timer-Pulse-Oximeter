# Recovery-Time Pulse Oximeter

**Technological Outcome — NZ Digital Technologies Assessment 2025**  
A timer project that uses an ESP32 and MAX30102 sensor to monitor SpO₂ (oxygen saturation) and heart rate during recovery after exertion.

## 🔍 Project Overview

This device tracks how quickly a person's vitals return to target levels after exercise. When the user presses Start, the timer begins and live readings are shown. The device automatically stops when target thresholds are met or a timeout is reached, then shows a summary of the session.

## 🎯 Features

- Real-time SpO₂ and heart rate monitoring
- Start/Stop/Reset button interface
- Auto-stop on target thresholds (e.g., SpO₂ ≥ 96%, HR within 10% of baseline)
- Session summary: time, min SpO₂, max HR, time-to-target
- OLED or LCD display output
- Optional data logging to EEPROM or over Serial (CSV)
- Buzzer feedback for target hit / timeout

## 🧩 Components Used

| Component      | Description                          |
|----------------|--------------------------------------|
| ESP32 Dev Board| Microcontroller with BLE/Wi-Fi       |
| MAX30102       | Pulse oximeter & heart rate sensor   |
| OLED/LCD1602   | Display for live values and summary  |
| Pushbuttons    | For Start / Stop / Reset             |
| Buzzer         | Audible cue when session ends        |
| (Optional) EEPROM | For data storage (or use SPIFFS) |

## 🖧 Wiring Diagram

_(Insert Fritzing or diagram here once available)_

- MAX30102 → SDA/SCL to ESP32 (I²C)
- Display → I²C (or parallel if LCD1602)
- Buttons → GPIO with pull-down resistors
- Buzzer → GPIO with limiting resistor

## 🚦 State Machine

```text
IDLE → RUNNING → TARGET_REACHED / TIMEOUT → SUMMARY → IDLE
```

## 📊 Example Output

```csv
Session,Start Time,Duration (s),Min SpO2 (%),Max HR (bpm),Target Time (s)
1,2025-08-22 10:31,42,93,132,36
2,2025-08-22 10:45,51,92,128,44
```

## 🔍 Assessment Alignment

| Requirement                        | Covered |
|------------------------------------|---------|
| Repeatable user control            | ✅ Start/Stop/Reset buttons |
| Output via display/CSV             | ✅ OLED or LCD, optional CSV |
| Use of advanced techniques         | ✅ ESP32 code, subsystem design, storage |
| Data captured/stored               | ✅ Session summary stored |
| Relevant implications addressed    | ✅ Privacy, safety, usability, sustainability |
| Testing & modification documented  | ✅ In dev log and black-box tests |

## 📹 Video Demo (Coming Soon)

## 🧪 Testing & Logs

- Weekly black-box testing with two users
- Dev logs with screenshots, code versions, test notes
- Interface behaviour and improvements tracked

### ✅ Stage 1 — LCD Smoke Test

**Goal:** Prove the I²C 1602A LCD works on ESP32 (text visible, no flicker).  
**Method:** `TestPrograms/lcd-tests/lcd-tests.ino`  
**Pass:** Clear text on both lines; stable for ≥60s.

**Evidence:**
- Photos:  
  ![LCD test 1](docs/photos/lcd-test-1.jpeg)  
  ![LCD test 2](docs/photos/lcd-test-2.jpeg)
- Video: [docs/videos/lcd-test.mov](docs/videos/lcd-test.mov)
- 
### ✅ Stage 2 — Button Test

**Goal:** Verify push button input using GPIO18 with `INPUT_PULLUP`.  
**Method:** Button wired between GPIO18 and GND. Code prints `Pressed`/`Released` via Serial Monitor.  
**Pass:** 50 presses with no false triggers.

**Evidence:**
- Screenshot:  
  ![Button test serial output](docs/photos/button-test-serial.png)
- Source: `TestPrograms/button-test/button-test.ino`

### ✅ Stage 3 — Buzzer Test

**Goal:** Confirm audible output using an active buzzer on GPIO19.  
**Method:** Pin 19 drives buzzer; code pulses 200 ms on / 800 ms off.  
**Pass:** Distinct beeps heard once per second.

**Evidence:**
- Clip: [docs/videos/buzzer-test.mov](docs/videos/buzzer-test.mov)
- Source: `TestPrograms/buzzer-test/buzzer-test.ino`

### ✅ Stage 4A — OLED UI (SSD1306)
**Goal:** Replace 1602 LCD with 0.91" I²C OLED for crisp text/graphics at 3.3 V.  
**Wiring:** VCC→3V3, GND→GND, SCL→GPIO22, SDA→GPIO21.  
**Evidence:**  

- Screenshot: ![OLED Mock-up](docs/photos/oled-mock-up.jpg)  
- Demo Video: [oled-mock-up.mov](docs/videos/oled-mock-up.mov)  

**Source:** `TestPrograms/oled-mock-ui/oled-mock-ui.ino`

### ✅ Stage 4B — MAX30102 Bring-up

**Goal:** Wire the MAX30102 to the ESP32, confirm I²C comms, and stream raw IR/RED before integrating SpO₂/HR.

**Method:**  
- Wire as below and run an I²C scanner with `Wire.begin(21,22)` to confirm address **0x57**.  
- Flash a minimal check sketch that prints IR/RED to Serial.  
- Adjust finger placement and LED amplitude as needed to get steady changes.

**Wiring:**  
- `VIN → 3V3`  
- `GND → GND`  
- `SDA → GPIO21 (D21)`  
- `SCL → GPIO22 (D22)`  
- `INT → GPIO19 (D19)` *(optional, active-low “data ready”)*

**Pass:**  
- I²C scanner shows **0x57**.  
- With a fingertip on the sensor, **IR/RED values change smoothly** in Serial Monitor.

**Evidence:**  
  ![LCD test 1](docs/photos/max30102-wired.jpg)  
- Photo: `docs/photos/max30102-wired.jpg`  
- Serial screenshot: `docs/photos/max30102-serial.png`

**Source:** `TestPrograms/max30102-check/max30102-check.ino`

**Notes:**  
- Keep the I²C bus at **3.3 V**. If your display runs at 5 V, make sure its I²C pull-ups are not tied to 5 V.  
- If **GPIO19** was used for the buzzer earlier, move the buzzer to **GPIO15** so `INT` can use **GPIO19**.


## 📄 License

This project uses open-source libraries. See [LICENSE](LICENSE) for more.


