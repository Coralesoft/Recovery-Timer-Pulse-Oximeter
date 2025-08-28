# Development Log - Recovery Timer Project

## Week 1 - Getting Started
**Monday** - Ordered parts online (ESP32, MAX30102, OLED display, buzzer)  
**Wednesday** - Parts arrived! Started with basic sensor test  
**Friday** - Got sensor working but readings are really noisy

## Week 2 - Individual Component Tests
**Monday** - Made button test program - works fine  
**Tuesday** - OLED display test - had wrong I2C address at first (was using 0x27 instead of 0x3C)  
**Wednesday** - Buzzer test - simple but works  
**Thursday** - Tried to get heart rate readings - harder than expected!  
**Friday** - Finally got heart rate algorithm working, had to use 100 sample buffer

Problems this week:
- Display wouldn't work until I found right address
- Heart rate readings take ages to stabilize  
- Finger detection threshold needed lots of tweaking

## Week 3 - Putting It Together
**Monday** - Made "everything together" test to check all parts work at same time  
**Tuesday** - Started main program - got basic state machine working  
**Wednesday** - Added baseline capture - works but takes practice to keep finger still  
**Thursday** - Exercise detection is tricky - tried different heart rate thresholds  
**Friday** - Recovery timing works! Takes about 30-60 seconds after jumping jacks

## Week 4 - Debugging and Polish
**Monday** - Readings too jumpy - added simple averaging (80% old + 20% new)  
**Tuesday** - Display was flickering - only update every 500ms now  
**Wednesday** - Session storage working - keeps last 20 sessions in memory  
**Thursday** - Added different beep patterns for different events  
**Friday** - Final testing and cleanup

## Problems I Solved
1. **Sensor readings jumping around** - Added smoothing with weighted average
2. **Finger detection unreliable** - Adjusted threshold from 5000 to 6500  
3. **Exercise detection not working** - Reduced heart rate increase from 15% to 8%
4. **Display flickering** - Limited update rate to twice per second
5. **Sometimes wouldn't detect finger return** - Added 3 second settling time

## Things I Learned
- I2C can have multiple devices on same pins (sensor and display)
- Heart rate algorithms need lots of samples to work properly  
- Debouncing buttons is important or you get multiple presses
- Serial monitor is really useful for debugging
- Taking time to test each part separately saves lots of time later

## Future Ideas
- Add WiFi to send data to phone app
- Maybe use different sensors for different exercises  
- Web interface to view history graphs
- Multiple users with different baselines

## Assessment Requirements Check
✅ Timer based project - measures recovery time  
✅ Data collection - stores heart rate, oxygen, recovery time  
✅ Repeatable - can do multiple sessions  
✅ Advanced programming - uses interrupts, algorithms, data structures  
✅ Subsystems - sensor input, display output, button control, data storage  
✅ Testing - separate test programs for each component  

**Total time:** About 4 weeks working 2-3 hours after school  
**Hardest part:** Getting stable heart rate readings  
**Most satisfying:** When it first detected my recovery automatically!

---
*Notes: Remember to mention SparkFun tutorials in credits - their examples really helped with the sensor setup*