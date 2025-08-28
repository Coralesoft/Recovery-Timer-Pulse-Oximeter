# Recovery Timer Pulse Oximeter - User Instructions

## Quick Start Guide

### What This Device Does
Measures how long it takes for your oxygen levels (SpO₂) and heart rate to return to normal after exercise or exertion.

### Basic Operation
The device has **one button** that controls everything:
- **Short Press**: Advance to next step or cancel
- **Long Press** (hold 1.5 seconds): Export data when idle

## Step-by-Step Usage

### 1. Getting Started
1. Power on the device
2. Wait for "READY [Press Start]" on display
3. Place your index finger firmly on the sensor (red light should be visible through finger)
4. Ensure you see readings (SpO₂ % and HR bpm) before starting

### 2. Running a Recovery Test

#### Phase 1: Baseline Capture
- **Press button once** to start baseline capture
- Keep finger still on sensor for 10 seconds
- Display shows countdown: "BASELINE: 10s" → "9s" → etc.
- Device beeps twice when baseline is captured
- **To cancel**: Press button during countdown

#### Phase 2: Exercise
- Remove finger from sensor
- Perform your exercise (jumping jacks, stairs, etc.)
- Aim for mild exertion - enough to elevate heart rate

#### Phase 3: Recovery Monitoring
- Immediately place finger back on sensor after exercise
- Device shows "TIMING: Xs" with elapsed seconds
- Keep finger still and breathe normally
- Device automatically stops when you've recovered to target levels
- **Manual stop**: Press button anytime to end session

#### Phase 4: Session Complete
- Display shows "DONE! Xs" with total recovery time
- Three beeps = automatic stop (target reached)
- One beep = manual stop
- **Press button** to return to READY for next session

## Understanding the Display

```
Line 1: Status/Timer
Line 2: SpO2: XX% HR: XXXbpm  
Line 3: Sensor status
```

### Target Thresholds
- **SpO₂**: Must reach 96% or higher
- **Heart Rate**: Must return within 10% of baseline
- Both conditions must be met for auto-stop

## Audio Feedback

| Sound | Meaning |
|-------|---------|
| 1 short beep | Action confirmed |
| 2 beeps | Baseline captured, timing started |
| 3 beeps | Target reached, auto-stop |
| Long beep | Error or timeout |

## Data Export

### To Export Session History
1. Return to READY screen
2. **Hold button for 1.5 seconds**
3. Two beeps confirm export
4. Open Serial Monitor (115200 baud) to see CSV data
5. Copy and paste into spreadsheet program

### CSV Format
```
Session,Timestamp,Duration,MinSpO2,MaxHR,BaselineHR,TargetReached
1,145632,45,94,132,72,Yes
```

## Troubleshooting

| Problem | Solution |
|---------|----------|
| "No finger" message | Adjust finger position, ensure good contact |
| Readings jumping | Keep finger very still, wait for stabilization |
| Won't start | Ensure valid readings visible before pressing start |
| Baseline fails | Keep finger still during 10-second capture |
| Never reaches target | May need longer recovery time, or adjust exercise intensity |


## Tips for Accurate Readings

1. **Finger placement**: Use index or middle finger, nail-side up
2. **Stay still**: Movement causes inaccurate readings
3. **Warm hands**: Cold fingers give poor readings
4. **Remove nail polish**: Can interfere with sensor
5. **Consistent pressure**: Don't squeeze sensor too tight
6. **Ambient light**: Avoid direct sunlight on sensor

## Session Storage

- Stores last 20 sessions automatically
- Oldest session deleted when memory full
- Data persists through power cycles
- Export regularly to preserve long-term records

## Typical Recovery Times

- Light exercise: 30-60 seconds
- Moderate exercise: 1-2 minutes  
- Intense exercise: 2-5 minutes
- Session timeout: 5 minutes maximum

## LED Indicators on Sensor

- Red light visible through finger = sensor active
- Brightness auto-adjusts for optimal readings
- If too dim/bright, device compensates automatically