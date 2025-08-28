# 3-Minute Demonstration Video Script
## Recovery Timer Pulse Oximeter - Year 12 Electronics Project

---

### Opening (0:00-0:20)
**[SHOT: Device on table with components visible]**

"Hi, I'm Max Brown from Wellington College, and this is my Year 12 electronics project - a Recovery Timer that measures how quickly your heart rate and oxygen levels return to normal after exercise.

Ever wondered how fit you really are? Recovery time is one of the best indicators, but commercial devices cost hundreds of dollars. So I built my own for under $50!"

---

### The Problem (0:20-0:40)
**[SHOT: Close-up of MAX30102 sensor and OLED display]**

"The challenge was creating a device that could:
- Accurately measure heart rate AND blood oxygen
- Automatically detect when you've exercised  
- Time your recovery without manual intervention
- Work reliably for different fitness levels

This meant solving real engineering problems, not just connecting components."

---

### How It Works - Demo Part 1 (0:40-1:20)
**[SHOT: Finger on sensor, display showing readings]**

"Here's how it works. First, place your finger on the sensor and press the button. 

*[Display shows: BASELINE: 10 seconds]*

It captures your resting heart rate and oxygen for 10 seconds. Mine's showing 72 bpm and 98% oxygen.

*[Remove finger]*

Now I'll do some exercise - just 20 jumping jacks to get my heart rate up.

*[SHOT: Do jumping jacks - can speed up/cut down]*

Okay, now I'm properly puffed! Let's see the recovery timing..."

---

### How It Works - Demo Part 2 (1:20-2:00)
**[SHOT: Place finger back, display shows RECOVERY]**

"When I put my finger back, it automatically detects I've exercised because my heart rate increased by more than 8%. Originally I had this at 15%, but testing showed that missed light exercise.

*[Display shows: RECOVERY: 15 sec, Target: O2-OK HR-NO]*

See those targets? It's tracking when my oxygen gets back to 96% and my heart rate within 12% of baseline. 

*[Wait for recovery - can speed up footage]*

There! 42 seconds to full recovery. It saves this data automatically - I can track my fitness improvement over time!"

---

### Technical Implementation (2:00-2:30)
**[SHOT: Circuit diagram on screen / breadboard close-up]**

"The technical side was challenging. I used:
- ESP32 for processing power - Arduino couldn't handle the SpO2 algorithms
- MAX30102 sensor for medical-grade accuracy
- Custom smoothing algorithms to reduce noise
- State machine programming for reliable operation

The biggest breakthrough was getting stable readings. Raw sensor data was jumping everywhere, so I implemented weighted averaging - 80% old value, 20% new. This balanced stability with responsiveness."

---

### Testing & Iteration (2:30-2:50)
**[SHOT: Show testing documentation/photos]**

"I didn't get it right first time! Through testing with family and friends, I discovered:
- My finger detection threshold was too sensitive - increased from 5000 to 6500
- Display was updating too fast and flickering - limited to twice per second  
- Different fitness levels needed different recovery targets

Each problem taught me something new about embedded systems and user experience design."

---

### Conclusion (2:50-3:00)
**[SHOT: Final device working, maybe with data export on screen]**

"This project combined electronics, programming, and health science into something actually useful. It accurately measures recovery time, stores 20 sessions, and exports data for tracking fitness over time.

Most importantly, I learned that good engineering isn't just making something work - it's iterating until it works well for real users.

Thanks for watching!"

---

## Alternative Shots/B-Roll Options

If main demonstration isn't possible, use these:
- Close-ups of sensor with finger detecting/not detecting
- Serial monitor showing data export
- Display showing different states (READY, BASELINE, RECOVERY, DONE)
- Circuit breadboard from different angles
- Code scrolling on computer screen
- Graph/chart of recovery times improving

## Key Points to Emphasize

1. **Problem-solving journey** - Not just the final product
2. **Iterative improvement** - Specific examples with numbers
3. **Real testing** - With actual users, not just yourself
4. **Technical depth** - But explained simply
5. **Practical application** - It actually works and is useful

## Technical Stats to Mention (pick 2-3)

- Accuracy: ±2 seconds recovery timing
- Detection: 95% exercise detection rate
- Processing: 100 samples for stable readings
- Memory: Stores 20 sessions
- Development: 6 weeks, 3-4 hours daily
- Testing: 10+ users, 5 exercise types
- Cost: Under $50 vs $200+ commercial

---

*Note: Keep energy high, speak clearly, and show enthusiasm for your project! This is YOUR creation - be proud of it!*