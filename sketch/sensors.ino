#include <Wire.h>
#include <vl53l4cd_class.h>
#include "config.h"

extern ModulinoThermo thermo;
extern int  lastDistanceMm;
extern bool presenceDetected;

// Direct VL53L4CD driver on Wire1 (UNO Q's Qwiic bus). No XSHUT pin needed.
VL53L4CD tof(&Wire1, -1);

// ---- Light smoothing state ----
static float lightSmoothed = 0;
static bool  lightInitialized = false;

static int analogReadAveraged(int pin, int samples = 10) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delayMicroseconds(200);
  }
  return sum / samples;
}

void initSensors() {
  // Init the VL53L4CD directly
  Wire1.begin();
  tof.begin();
  tof.InitSensor(0x29);
  tof.VL53L4CD_SetRangeTiming(50, 0);  // 50ms timing budget, continuous
  tof.VL53L4CD_StartRanging();

  // Pre-warm the light smoothing filter
  for (int i = 0; i < 20; i++) {
    int raw = analogReadAveraged(LIGHT_PIN, 10);
    if (!lightInitialized) { lightSmoothed = raw; lightInitialized = true; }
    lightSmoothed = 0.3f * raw + 0.7f * lightSmoothed;
    delay(10);
  }
}

float getTempC()    { return thermo.getTemperature(); }
float getHumidity() { return thermo.getHumidity(); }

int getLightLevel() {
  int raw = analogReadAveraged(LIGHT_PIN, 10);
  if (!lightInitialized) { lightSmoothed = raw; lightInitialized = true; }
  lightSmoothed = 0.3f * raw + 0.7f * lightSmoothed;

  int pct = map((int)lightSmoothed, 0, 2500, 0, 100);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

int  getDistanceMm() { return lastDistanceMm; }
bool getPresence()   { return presenceDetected; }

void updateDistance() {
  uint8_t dataReady = 0;
  tof.VL53L4CD_CheckForDataReady(&dataReady);
  if (dataReady) {
    VL53L4CD_Result_t results;
    tof.VL53L4CD_GetResult(&results);
    tof.VL53L4CD_ClearInterrupt();

    // Range status 0 = valid, other values = various warnings/errors
    if (results.range_status == 0 && results.distance_mm > 0) {
      int mm = results.distance_mm;
      if (mm < 4000) {
        lastDistanceMm = mm;
        presenceDetected = (mm < PRESENCE_THRESHOLD_MM);
      }
    } else {
      lastDistanceMm = 9999;
      presenceDetected = false;
    }
  }
}