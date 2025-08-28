# Development Log - Recovery Timer Project

## Project Overview
This development log documents the week-by-week progress of my Year 12 Electronics Recovery Timer project, showing the iterative improvement process required for Excellence level assessment.

## Pre-Development Phase

### Week -1: Planning and Component Research
**Goals:** Understand project requirements, research components
- Studied AS91894 assessment criteria for Excellence level
- Researched heart rate sensor options (MAX30102 vs cheaper alternatives)
- Decided on ESP32 vs Arduino Uno based on processing power needs
- Ordered components: ESP32, MAX30102, OLED display, buzzer, jumper wires

**Key Decision:** Chose MAX30102 for medical-grade accuracy over basic pulse sensors

## Development Phase 1: Component Testing

### Week 1: Individual Component Validation
**Monday - Tuesday:**
- ESP32 setup and Arduino IDE configuration
- Basic "Hello World" and LED blink tests
- Installed required libraries (Adafruit, SparkFun)

**Wednesday - Thursday:**  
- **Sensor Test:** Created `experiments/basic_sensor_test/`
  - SUCCESS: IR readings 1000 (no finger) → 15000+ (with finger) 
  - PROBLEM: Raw readings very noisy, jumping around constantly
  - SOLUTION: Learned algorithm needs 100-sample buffer for stability

**Friday:**
- **Button Test:** Created `experiments/button_test/`
  - SUCCESS: INPUT_PULLUP works perfectly, no external resistor needed
  - Learned proper debouncing techniques

**Weekend:**
- **Display Test:** Created `experiments/oled_test/`  
  - PROBLEM: Display wouldn't initialize - spent hours debugging
  - BREAKTHROUGH: Wrong I2C address! Changed from 0x27 to 0x3C
  - SUCCESS: Clear display working with proper text layout

### Week 2: Algorithm Development
**Monday:**
- **Buzzer Test:** Simple beep patterns working immediately
- Started heart rate algorithm integration

**Tuesday - Wednesday:**
- **Major Challenge:** SpO2 algorithm implementation
- PROBLEM: Maxim algorithm documentation confusing
- SOLUTION: Found SparkFun example code and adapted it
- Learned algorithm needs continuous 100 samples, not just single readings

**Thursday - Friday:**
- **Heart Rate Test:** Created `experiments/heart_rate_test/`
- SUCCESS: Getting heart rate and SpO2 readings
- PROBLEM: Takes 30+ seconds to get stable readings
- INSIGHT: This is normal for accurate pulse oximetry - patience required

**Key Milestone:** All individual components working correctly

## Development Phase 2: System Integration

### Week 3: Building the Main System
**Monday:**
- **Integration Test:** Created `experiments/everything_together/`
- SUCCESS: All components work simultaneously without conflicts
- I2C bus sharing between sensor and display works perfectly

**Tuesday - Wednesday:**
- Started main recovery timer program
- Implemented basic state machine: IDLE → BASELINE → EXERCISE → RECOVERY → RESULTS
- **First Major Problem:** State transitions not working smoothly

**Thursday - Friday:**  
- Fixed state machine logic with boolean flags instead of complex enums
- **Breakthrough:** Baseline capture working reliably in 10 seconds
- Exercise detection implemented but very unreliable

**Weekend Analysis:**
- Finger detection threshold too sensitive (getting false positives)
- Exercise detection threshold too high (missing light exercise)

### Week 4: Threshold Optimization Through Testing

**Monday - Tuesday:** Finger Detection Tuning
- **Initial Setting:** threshold = 5000  
- **Problem:** False positives in bright rooms (IR = 4000+ without finger)
- **Testing Method:** Tested 10 different people in various lighting
- **Data Collected:** No finger: 2000-4000, With finger: 8000-20000
- **Solution:** Increased threshold to 6500
- **Result:** 100% reliable finger detection, no false positives

**Wednesday - Thursday:** Exercise Detection Optimization
- **Initial Setting:** 15% heart rate increase required
- **Problem:** Missed moderate exercise (stairs, light jogging)
- **Testing Method:** 5 exercise types with 4 different people
- **Data Analysis:** Light exercise only increased HR by 8-12%
- **Solution:** Reduced threshold to 8% increase  
- **Result:** Detection rate improved from 60% to 95%

**Friday:** Display Improvement
- **Problem:** Users complained display was "jumpy and hard to read"
- **Root Cause:** Updating display every loop cycle (~50Hz)
- **Solution:** Limited updates to 500ms intervals (2Hz)
- **Result:** Stable, readable display that conserves power

**Weekend:** Data smoothing implementation
- **Problem:** Even with slower updates, readings still noisy
- **Solution:** Exponential moving average (80% old + 20% new)
- **Result:** Smooth readings that still respond quickly to changes

## Development Phase 3: Refinement and Testing

### Week 5: User Interface Polish
**Monday:**
- Added different beep patterns for different events
- Short beep: start actions
- Medium beep: success
- Long beep: errors/cancellation
- Very long beep: timeouts

**Tuesday - Wednesday:**
- Session storage implementation (20 sessions maximum)
- Data export functionality via serial monitor
- Long button press (2 seconds) triggers export

**Thursday - Friday:**
- **Real-world testing with family members**
- Collected actual recovery data from different fitness levels
- Fine-tuned recovery targets based on testing

### Week 6: Final Testing and Documentation
**Monday - Tuesday:**
- Comprehensive system testing
- Edge case handling (very fit users, sensor errors)
- **Final threshold validation:** 8% exercise detection works across all fitness levels

**Wednesday - Thursday:**
- User testing with 3 different people
- Collected feedback and made minor UI improvements
- Added clearer on-screen prompts based on user confusion

**Friday:**
- Final code cleanup and commenting
- Performance verification: recovery timing accurate to ±2 seconds
- Created comprehensive circuit diagram (ESP32-Diagram-Final.svg) showing all components
- All assessment criteria verified as met

## Key Milestones and Breakthroughs

### Technical Breakthroughs:
1. **Week 1:** OLED I2C address discovery (0x3C not 0x27)
2. **Week 2:** Understanding 100-sample buffer requirement for accurate readings  
3. **Week 4:** 8% exercise threshold optimization based on real data
4. **Week 4:** Exponential moving average smoothing implementation
5. **Week 5:** Reliable session storage and export functionality

### Problem-Solving Examples:
1. **Noisy Readings** → Smoothing algorithm (80/20 weighted average)
2. **False Finger Detection** → Threshold optimization (5000 → 6500)
3. **Missed Light Exercise** → Lower detection threshold (15% → 8%)
4. **Flickering Display** → Update rate limiting (50Hz → 2Hz)
5. **Button Bounce** → Software debouncing (50ms delay)

## Testing Data That Drove Improvements

### Finger Detection Threshold Testing:
- **5000 threshold:** 30% false positives in bright light
- **6000 threshold:** 10% false positives  
- **6500 threshold:** 0% false positives, 100% detection reliability
- **7000 threshold:** Started missing fingers with poor circulation

### Exercise Detection Testing:
- **15% threshold:** Detected only 60% of exercise attempts
- **12% threshold:** Detected 75% of exercise attempts
- **10% threshold:** Detected 85% of exercise attempts  
- **8% threshold:** Detected 95% of exercise attempts
- **5% threshold:** Too many false positives from normal variations

### Display Update Rate Testing:
- **Every loop (50Hz):** Users: "jumpy, hard to read, distracting"
- **10Hz:** Users: "better but still flickery"
- **5Hz:** Users: "much better, readable"
- **2Hz (500ms):** Users: "perfect, stable and responsive"
- **1Hz:** Users: "too slow, seems broken"

## Final System Specifications

### Achieved Performance:
- **Finger Detection:** 100% reliable (tested with 10+ users)
- **Exercise Detection:** 95% success rate across fitness levels
- **Recovery Timing:** ±2 second accuracy verified against stopwatch
- **Baseline Capture:** Consistent results in 10 seconds
- **Data Storage:** Reliable 20-session memory with export

### User Feedback Summary:
- ✅ "Easy to understand and use"
- ✅ "Audio feedback is really helpful"  
- ✅ "Works reliably once you learn finger placement"
- ⚠️ "Takes practice to get stable readings" (inherent sensor limitation)

## Assessment Criteria Evidence

### Iterative Improvement:
- 5+ major threshold adjustments based on testing data
- Multiple UI improvements based on user feedback
- Component selection changes (LCD → OLED)
- Algorithm enhancements (smoothing, debouncing)

### Justified Design Decisions:
- ESP32 vs Arduino: Processing power for real-time calculations
- MAX30102 vs basic sensors: Medical-grade accuracy requirement
- OLED vs LCD: Power consumption and readability
- Single button vs multiple: Simplicity during exercise

### Refined Outcome:
- Final system works reliably across different users and fitness levels
- All technical problems identified and solved through testing
- User interface refined based on real user feedback
- Performance meets all project requirements

---
**Total Development Time:** 6 weeks (approximately 3-4 hours per day after school)
**Most Challenging Aspect:** Getting stable, reliable sensor readings
**Most Rewarding Moment:** First successful automatic recovery detection
**Key Learning:** Iterative testing and refinement is essential for Excellence level outcomes