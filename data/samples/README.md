# Sample Data Files
## Recovery Timer Data Export Examples

---

## What's in this folder?

This folder contains example data exports from the Recovery Timer device, showing what kind of information it collects and how it's formatted.

## Files Included

### sample_export_data.csv
- **What it is**: Example export from serial monitor after 10 exercise sessions
- **How it's created**: Long press button (2 seconds) on main screen, copy from Serial Monitor
- **Format**: CSV (Comma Separated Values) - opens in Excel or Google Sheets

## Data Fields Explained

| Field | What it means | Example |
|-------|--------------|---------|
| **Session** | Session number (1-20) | 1, 2, 3... |
| **Time** | Recovery time in seconds | 45s |
| **Min O2** | Lowest oxygen during recovery | 94% |
| **Max HR** | Highest heart rate during recovery | 142 bpm |
| **Rest HR** | Baseline heart rate before exercise | 72 bpm |
| **Auto Stop** | Did it stop automatically (Yes) or manually (No) | Yes |

## What the Data Shows

Looking at the sample data, you can see:
- **Recovery times range from 35-52 seconds** - Pretty consistent!
- **Oxygen drops to 93-96%** during recovery - Normal range
- **Heart rates reach 134-145 bpm** - Moderate exercise level
- **Resting heart rates 68-73 bpm** - Healthy range for teenagers
- **Most sessions auto-stopped** - The detection algorithm works!

## Interesting Patterns

1. **Session 5 (35 seconds)** - Fastest recovery, lowest max HR (135)
   - Probably light exercise or improving fitness

2. **Session 6 (48 seconds)** - Manual stop (user pressed button)
   - Maybe moved finger or got impatient

3. **Sessions getting faster over time?** 
   - Could indicate fitness improvement!
   - This is why tracking data is useful

## How to Use This Data

### For Testing
- Check recovery times are realistic (30-90 seconds typical)
- Verify oxygen levels stay in safe range (>93%)
- Ensure heart rates are physiologically possible

### For Analysis  
- Import into Excel to create graphs
- Track fitness improvement over time
- Compare different exercise types

### For Debugging
- If times are too short (<20s), threshold might be too sensitive
- If oxygen reads >100% or <90%, sensor calibration issue
- If no auto-stops, recovery targets might be too strict

## Real vs Sample Data

This sample data is based on my actual testing but simplified for demonstration:
- Real data has timestamps (milliseconds since start)
- Real export includes more decimal places
- Actual sessions might have more variation

## Creating Your Own Data

1. Complete several recovery timer sessions
2. On main screen, hold button for 2 seconds
3. Open Serial Monitor (115200 baud)
4. Copy the exported table
5. Paste into a text file and save as .csv

---

## Future Data Ideas

For version 2, I'd like to add:
- Exercise type (jumping jacks, stairs, etc.)
- Time of day
- Temperature/humidity
- User profiles for family members

---

*This data helps prove the device actually works and produces meaningful measurements!*