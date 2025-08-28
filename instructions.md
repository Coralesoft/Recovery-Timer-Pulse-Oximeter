# Instructions - Recovery Timer

## What it does
This device times how long it takes for your heart rate and oxygen to get back to normal after exercise.

## How to use
1. **Start** - Place finger on sensor, press button
2. **Baseline** - Keep finger still for 10 seconds while it records your normal levels  
3. **Exercise** - Remove finger and do some exercise (jumping jacks, run upstairs, etc)
4. **Recovery** - Place finger back on sensor, it automatically starts timing
5. **Done** - When you're back to normal levels it stops and shows your time

## Button controls
- **Short press** - Start session, stop timing, go to next screen
- **Long press** (hold 2 seconds) - Export your data over serial monitor

## What the display shows
- **READY- Press to start** - Put finger on and press to start
- **BASELINE: X seconds** - Recording your normal levels (X seconds left)
- **Do exercise, then** / **place finger back!** - Self explanatory!
- **Ready to start timing?** - Shows when finger is back on sensor
- **RECOVERY: X sec** - Timing your recovery (X seconds so far)
- **DONE! Time: Xs** - Finished! Took X seconds to recover

## Understanding the readings
- **O2: XX%** - Your blood oxygen level (normal is 95-100%)
- **HR: XXX bpm** - Your heart rate in beats per minute
- **Target: O2-OK HR-OK** - Shows which recovery targets you've reached (or O2-NO HR-NO if not met)

## Recovery targets
- **Oxygen**: Must get back to 96% or higher  
- **Heart Rate**: Must get within 12% of your baseline

## Tips for good readings
- Keep finger still on sensor (movement makes readings jump)
- Don't press too hard on sensor
- Make sure finger covers the sensor completely  
- Clean the sensor if readings seem wrong
- Takes 10-20 seconds for readings to stabilize

## Exporting your data
Hold the button for 2 seconds when on the ready screen. Open the Serial Monitor in Arduino IDE to see your session history in a table format.