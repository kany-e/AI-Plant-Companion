#include <Arduino_RouterBridge.h>
#include <Arduino_Modulino.h>
#include <Arduino_LED_Matrix.h>
#include <math.h>
#include "config.h"

// ---- Global hardware objects (defined once here) ----
ModulinoThermo    thermo;
ModulinoMovement  movement;
Arduino_LED_Matrix matrix;

// ---- Shared state (set by sensor functions, read by bridge API) ----
int  lastDistanceMm   = 9999;
bool presenceDetected = false;
int  lastMovementEvent = 0;

// ---- Forward declarations (functions defined in other .ino files) ----
void initDisplay();
void scrollMessage(const char* msg, int frameDelay);

void initSensors();
int  getLightLevel();
void updateDistance();

void initMovement();
void updateMovement();
void captureMovementBaseline();
bool isBaselineReady();

void registerBridgeAPI();

// ---- Display rotation ----
unsigned long lastDisplayUpdate = 0;
int displayMode = 0;

void setup() {
  Bridge.begin();
  Monitor.begin(9600);

  analogReadResolution(12);

  Modulino.begin(Wire1);
  thermo.begin();
  movement.begin();

  initDisplay();
  initSensors();
  initMovement();
  registerBridgeAPI();
}

void loop() {
  updateDistance();
  updateMovement();

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

  delay(10);
}
