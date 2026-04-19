# 🌱 AI Plant Companion

Your houseplant, but it can tell you how it feels.

AI Plant Companion is a smart plant monitoring system built on the Arduino UNO Q. It combines real-time environmental sensors with Claude's multimodal AI to translate raw sensor data into plant-centered updates like *"I'm comfortable"*, *"It's too hot here"*, or *"I could use a mist."* Instead of just showing numbers, it gives your plant a voice.

## What it does

- **Identifies your plant from a photo** — upload a picture in the web dashboard and Claude identifies the species
- **Researches care requirements automatically** — Claude returns ideal temperature, humidity, and light ranges for that specific plant
- **Monitors the environment in real time** — tracks temperature, humidity, light level, and presence (someone near the plant)
- **Translates readings into plant-centered language** — the dashboard shows things like *"The air is too dry — I could use a mist"* instead of raw percentages
- **Diagnoses health issues from photos** — upload a close-up and Claude identifies visible problems (yellowing, spots, pests) and suggests treatments
- **Presence-activated LCD** — the little screen next to the plant only lights up when you're nearby, then shows the plant's current mood
- **Works offline** — once a plant profile is pushed to the Arduino, the LCD keeps working with plant-specific thresholds even if the Linux side is down

## Hardware

Built for the **Arduino UNO Q** with the following sensors:

| Component | Purpose |
|-----------|---------|
| Modulino Thermo | Temperature + humidity |
| Photoresistor (on A1, with 10kΩ divider) | Ambient light level |
| VL53L4CD ToF sensor (Qwiic) | Distance / presence detection |
| IskakINO 16×2 I²C LCD | At-a-glance plant status display |
| Built-in 8×13 LED matrix | Scrolling readings display |

## Architecture

The UNO Q has two sides that work together:

**Arduino side (C++)** — real-time sensor reads, LED matrix animations, LCD updates. Doesn't talk to the internet directly.

**Linux side (Python)** — web dashboard, Claude API calls, plant profile storage. Pushes the plant's ideal comfort ranges to the Arduino so the LCD can show plant-specific messages without a round-trip.

The two sides communicate over the Arduino Router Bridge. Python polls sensor values; Python pushes the plant profile.

```
┌─────────────────┐         ┌──────────────────┐         ┌──────────────┐
│  Web Dashboard  │ ◄─HTTP─►│   Python side    │ ◄─API─► │ Anthropic    │
│  (index.html)   │         │   (main.py)      │         │ Claude API   │
└─────────────────┘         └──────────────────┘         └──────────────┘
                                     │
                                 RouterBridge
                                     │
                            ┌────────▼─────────┐
                            │  Arduino side    │
                            │  (sketch.ino)    │
                            └──────────────────┘
                                     │
                        ┌────────────┼────────────┐
                        │            │            │
                    Sensors       LED Matrix     LCD
```

## Getting started

### Prerequisites

- Arduino UNO Q
- All the sensors listed above
- [Arduino App Lab](https://docs.arduino.cc/software/app-lab/) installed
- An Anthropic API key from [console.anthropic.com](https://console.anthropic.com) (put a few dollars of credit on the account)

### Setup

1. **Clone the repo** into your Arduino Apps folder

   ```bash
   cd ~/ArduinoApps
   git clone https://github.com/kany-e/AI-Plant-Companion.git
   ```

2. **Wire the sensors**
   - Photoresistor to pin A1 with a 10kΩ resistor to ground
   - Modulino Thermo and VL53L4CD to the Qwiic bus
   - LCD I²C to the same bus (default address 0x27)

3. **Add your API key** in `python/.env`:

   ```
   ANTHROPIC_API_KEY=sk-ant-api03-your-key-here
   ```

   Don't commit this file. It's already in `.gitignore`.

4. **Open the app in App Lab** and click Run. App Lab compiles the sketch, installs Python dependencies, and starts both sides.

5. **Open the dashboard** — App Lab prints a URL like `http://<uno-q-ip>:7000`. Open it on any device on the same network.

### First-time use

1. On first launch, the dashboard jumps to the Setup tab
2. Either upload a photo of your plant (AI identifies it) or type the name directly
3. Give your plant a nickname
4. Click **Save & continue** — Claude researches care info and saves the profile
5. The dashboard switches to showing live readings with plant-specific mood feedback

## Project structure

```
AI-Plant-Companion/
├── index.html              # Web dashboard served by the WebUI brick
├── main.py                 # Python backend: API endpoints, Claude calls, sensor polling
├── plantbrain.py           # Claude API wrapper (identify, research, diagnose, evaluate)
├── requirements.txt        # Python dependencies (python-dotenv, fastapi)
├── python/.env             # Your API key (not committed)
└── sketch/
    ├── sketch.ino          # Arduino main loop and hardware init
    ├── sketch.yaml         # Library dependencies for the sketch
    ├── config.h            # Pin assignments and default thresholds
    ├── sensors.ino         # Photoresistor, VL53L4CD, temp/humidity readers
    ├── display.ino         # 8×13 LED matrix scrolling text
    ├── lcd.ino             # 16×2 LCD display with presence-activated backlight
    ├── profile.ino         # In-RAM plant profile cache pushed by Python
    └── bridge_api.ino      # Bridge endpoint registration
```

## How the plant profile sync works

1. User picks a plant in the web UI
2. Python calls `plantbrain.research_plant()` → Claude returns a JSON care profile
3. Python saves it to `/tmp/plant.json` and calls `Bridge.call("setPlantProfile", ...)` on the Arduino
4. Arduino stores the comfort ranges in RAM and the LCD starts using them
5. On reboot of either side, Python re-pushes the profile within a second

If the Linux side is down, the Arduino falls back to generic indoor-plant thresholds (16–28°C, 35–75% humidity, 20–90% light) so the LCD keeps showing sensible messages.

## Customizing

- **Add or change plant-centered prompts** → edit `evaluate_environment()` in `plantbrain.py`
- **Adjust the light sensor calibration window** → change `CALIBRATION_MS` in `sensors.ino`
- **Change the LCD on-time after presence** → edit `LCD_ON_DURATION_MS` in `lcd.ino`
- **Use a different Claude model** → update `VISION_MODEL` and `TEXT_MODEL` at the top of `plantbrain.py`

## Known limitations

- **Photoresistor accuracy** is approximate — it's a rough brightness detector, not a lux meter. Good enough for "is the plant in dark / dim / bright light?" but not precise lighting measurements.
- **VL53L4CD range** is optimized for indoor distances under 1 meter. Farther than that and readings get unreliable.
- **`/tmp/plant.json`** doesn't persist across full reboots on all Linux configurations. Change `PLANT_FILE` in `main.py` to a persistent path if you need it.

## Built with

- [Arduino UNO Q](https://docs.arduino.cc/hardware/uno-q) and [App Lab](https://docs.arduino.cc/software/app-lab/)
- [Anthropic Claude](https://www.anthropic.com) — Opus 4.7 for vision, Sonnet 4.6 for text
- [FastAPI](https://fastapi.tiangolo.com) (via the WebUI brick)
- [python-dotenv](https://github.com/theskumar/python-dotenv) for secrets management

## License

MIT# AI-Plant-Companion
Your personal pet plant
