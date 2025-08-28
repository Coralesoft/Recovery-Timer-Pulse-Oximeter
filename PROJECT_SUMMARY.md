# Recovery Timer Pulse Oximeter - Project Summary
## Year 12 Electronics Project | Max Brown | Wellington College

---

## 📊 Project Statistics

### Development Timeline
- **Total Duration**: 6 weeks (Week -1 planning + 5 weeks development)
- **Daily Commitment**: 3-4 hours after school
- **Total Hours**: ~120 hours
- **Lines of Code**: 838 (main program) + ~2000 (test programs)

### Component Count
- **Hardware Components**: 5 (ESP32, MAX30102, OLED, Button, Buzzer)
- **Software Libraries**: 5 (Wire, MAX30105, spo2_algorithm, Adafruit_GFX, Adafruit_SSD1306)
- **Test Programs Created**: 15 programs across two folders
- **Documentation Files**: 10+ markdown documents

---

## 🎯 Performance Metrics

### Accuracy & Reliability
- **Recovery Timing Accuracy**: ±2 seconds
- **Exercise Detection Rate**: 95% success
- **Finger Detection Reliability**: 100% (after threshold optimization)
- **False Positive Rate**: 0% (with 6500 threshold)
- **Data Storage Capacity**: 20 sessions

### Technical Specifications  
- **Sampling Rate**: 100 samples/second
- **Display Update Rate**: 2 Hz (optimized from 50 Hz)
- **Processing Speed**: 240 MHz dual-core ESP32
- **Memory Usage**: ~15KB of 520KB available RAM
- **Power Consumption**: ~150mA @ 3.3V

---

## 🔄 Key Iterations

### Major Improvements Through Testing
1. **Finger Detection**: 5000 → 6500 (30% false positives eliminated)
2. **Exercise Threshold**: 15% → 8% (detection rate improved from 60% to 95%)  
3. **Display Updates**: 50Hz → 2Hz (eliminated flickering)
4. **Signal Smoothing**: Raw → 80/20 weighted average (stable readings)
5. **Component Choice**: LCD → OLED (better visibility, lower power)

### Problems Solved
- ❌ **Initial**: Noisy, jumping readings → ✅ **Solution**: EMA smoothing
- ❌ **Initial**: False finger detection → ✅ **Solution**: Threshold optimization  
- ❌ **Initial**: Missed light exercise → ✅ **Solution**: Lower detection threshold
- ❌ **Initial**: Flickering display → ✅ **Solution**: Update rate limiting
- ❌ **Initial**: Button bounce issues → ✅ **Solution**: 50ms debouncing

---

## 👥 Testing Coverage

### User Testing
- **Total Testers**: 10+ people
- **Age Range**: 9-45 years
- **Health Conditions**: Including heart condition (shows device adaptability)
- **Exercise Types Tested**: 5 (jumping jacks, stairs, burpees, jogging, squats)
- **Total Test Sessions**: 50+

### Test Results Summary
- **Fastest Recovery**: 38 seconds (Dad, regular fitness)
- **Slowest Recovery**: 65 seconds (Lillybelle, heart condition)
- **Average Recovery**: 41 seconds
- **Most Common Feedback**: "Easy to use once you learn finger placement"

---

## 💡 Innovation & Learning

### Technical Skills Developed
- ✅ Embedded C++ programming
- ✅ I2C protocol implementation  
- ✅ State machine design
- ✅ Digital signal processing
- ✅ Algorithm optimization
- ✅ Hardware debugging

### Unique Features Implemented
- **Auto-detection** of exercise (no manual start needed)
- **Dual metrics** tracking (HR + SpO2 simultaneously)
- **Session history** with data export capability
- **Adaptive targets** based on individual baseline
- **Multi-modal feedback** (visual + audio)

---

## 📈 Success Metrics

### Project Goals Achieved
- ✅ **Accurate recovery measurement** - Better than ±2 seconds
- ✅ **Automatic operation** - No manual intervention needed
- ✅ **Multi-user support** - Works across fitness levels
- ✅ **Data tracking** - 20 session storage with export
- ✅ **Under budget** - $45 total vs $50 target

### Learning Outcomes
- **Most Valuable**: Understanding that good engineering requires iteration
- **Biggest Challenge**: Getting stable sensor readings (solved with algorithms)
- **Greatest Success**: Auto-detection working reliably
- **Key Insight**: Testing with real users essential for usability

---

## 🏆 What Makes This Project Special

1. **Practical Application** - Actually useful for fitness tracking
2. **Original Implementation** - Not just following a tutorial
3. **Evidence-Based Design** - Every decision backed by testing data
4. **Complete Documentation** - Full development journey recorded
5. **Working Product** - Not just a prototype, but finished device

---

## 📝 Documentation Completeness

- ✅ 838 lines of commented main code
- ✅ 15 test programs showing development process
- ✅ 10+ documentation files totaling 50+ pages
- ✅ 3 demonstration videos
- ✅ 5 progress photos
- ✅ Complete circuit diagram with all components
- ✅ Sample data demonstrating functionality

---

## 🚀 Future Potential

### Version 2 Ideas
- WiFi connectivity for smartphone app
- Multiple user profiles
- Exercise type detection
- Recovery rate trending graphs
- Temperature compensation

### Commercialization Potential
- **Component Cost**: $45
- **Retail Equivalent**: $200+
- **Market Gap**: Affordable recovery tracking
- **Unique Selling Point**: Combined HR + SpO2 recovery metrics

---

## 💭 Final Reflection

This project transformed from "just make something with a timer" into a genuinely useful device that solves a real problem. The journey from noisy, unreliable readings to a polished product that works for different users taught me that engineering is as much about iteration and testing as it is about initial design.

**Most importantly**: I learned that the best solutions come from understanding the problem deeply, testing with real users, and being willing to completely redesign when something isn't working.

---

*"Good engineering isn't making something work once - it's making it work reliably for everyone."*

**- Max Brown, Year 12, Wellington College**