#include <Arduino_RouterBridge.h>
#include <Arduino_Modulino.h>
#include <Arduino_LED_Matrix.h>
#include <math.h>
#include "config.h"

// ---- Global hardware objects (defined once here) ----
ModulinoThermo     thermo;
Arduino_LED_Matrix matrix;

// ---- Shared state (set by sensor functions, read by bridge API) ----
int  lastDistanceMm   = 9999;
bool presenceDetected = false;

// ---- Forward declarations (functions defined in other .ino files) ----
void initDisplay();
void scrollMessage(const char* msg, int frameDelay);

void initSensors();
int  getLightLevel();
void updateDistance();

void initLCD();
void updateLCD(float tempC, float humidity, int light, bool presence);

void registerBridgeAPI();

// ---- Display rotation ----
unsigned long lastDisplayUpdate = 0;
unsigned long lastLcdUpdate     = 0;
int displayMode = 0;

void setup() {
  Bridge.begin();
  Monitor.begin(9600);

  analogReadResolution(12);

  Modulino.begin(Wire1);
  thermo.begin();

  initDisplay();
  initSensors();
  initLCD();
  registerBridgeAPI();
}

void loop() {
  updateDistance();

  // LED matrix: scroll one of two messages every 500ms
  if (millis() - lastDisplayUpdate >= 500) {
    lastDisplayUpdate = millis();

    char msg[32];
    if (displayMode == 0) {
      int t = (int)thermo.getTemperature();
      int h = (int)thermo.getHumidity();
      snprintf(msg, sizeof(msg), " %dC %d%% ", t, h);
    } else {
      int l = getLightLevel();
      snprintf(msg, sizeof(msg), " L %d%% ", l);
    }

    scrollMessage(msg, 80);
    displayMode = (displayMode + 1) % 2;
  }

  // LCD: refresh every 1s
  if (millis() - lastLcdUpdate >= 1000) {
    lastLcdUpdate = millis();
    updateLCD(
      thermo.getTemperature(),
      thermo.getHumidity(),
      getLightLevel(),
      presenceDetected
    );
  }

  delay(10);
}
