#include <Wire.h>
#include <vl53l4cd_class.h>
#include "config.h"

extern ModulinoThermo thermo;
extern int  lastDistanceMm;
extern bool presenceDetected;

VL53L4CD tof(&Wire1, -1);

// ---- Light smoothing state ----
static float lightSmoothed = 0;
static bool  lightInitialized = false;

// ---- Auto-calibration state ----
static const unsigned long CALIBRATION_MS = 30000;
static unsigned long lightStartMs = 0;
static int   lightRawMin = 4095;
static int   lightRawMax = 0;
static bool  calibrationDone = false;

// ---- 10-second rolling average ----
static const int AVG_WINDOW = 10;
static int   lightBuffer[AVG_WINDOW];
static int   lightBufferCount = 0;
static int   lightBufferHead  = 0;
static unsigned long lastBufferPush = 0;
static const unsigned long PUSH_INTERVAL_MS = 1000;

// ---- Deadband: only update displayed value if it changed meaningfully ----
// This prevents the percentage from jittering ±1% from ADC noise alone.
static int   lastReportedPct = -1;
static const int DEADBAND_PCT = 2;

// Median-of-N reader: collects N samples, sorts, returns the middle one.
// Median is more robust to single-reading glitches than a plain mean.
static int analogReadMedian(int pin, int samples = 15) {
  int readings[25];
  if (samples > 25) samples = 25;
  for (int i = 0; i < samples; i++) {
    readings[i] = analogRead(pin);
    delayMicroseconds(200);
  }
  // Simple insertion sort — fast enough for 15 elements
  for (int i = 1; i < samples; i++) {
    int key = readings[i];
    int j = i - 1;
    while (j >= 0 && readings[j] > key) {
      readings[j + 1] = readings[j];
      j--;
    }
    readings[j + 1] = key;
  }
  return readings[samples / 2];
}

void initSensors() {
  Wire1.begin();
  tof.begin();
  tof.InitSensor(0x29);
  tof.VL53L4CD_SetRangeTiming(10, 30);
  tof.VL53L4CD_StartRanging();

  // Pre-warm the smoothing filter with median readings
  for (int i = 0; i < 20; i++) {
    int raw = analogReadMedian(LIGHT_PIN, 15);
    if (!lightInitialized) { lightSmoothed = raw; lightInitialized = true; }
    // Slower smoothing (0.15 weight on new) for stability
    lightSmoothed = 0.15f * raw + 0.85f * lightSmoothed;
    delay(10);
  }

  lightRawMin = (int)lightSmoothed;
  lightRawMax = (int)lightSmoothed + 1;
  lightStartMs = millis();
  calibrationDone = false;

  int span0 = lightRawMax - lightRawMin;
  if (span0 < 50) span0 = 50;
  int pct0 = (int)(((long)((int)lightSmoothed - lightRawMin) * 100L) / span0);
  if (pct0 < 0) pct0 = 0;
  if (pct0 > 100) pct0 = 100;
  for (int i = 0; i < AVG_WINDOW; i++) lightBuffer[i] = pct0;
  lightBufferCount = 0;
  lightBufferHead  = 0;
  lastBufferPush   = 0;
  lastReportedPct  = pct0;
}

float getTempC()    { return thermo.getTemperature(); }
float getHumidity() { return thermo.getHumidity(); }

static int computeLightPct() {
  int raw = analogReadMedian(LIGHT_PIN, 15);
  if (!lightInitialized) { lightSmoothed = raw; lightInitialized = true; }

  // Slower smoothing for a more stable value
  lightSmoothed = 0.15f * raw + 0.85f * lightSmoothed;

  int smoothed = (int)lightSmoothed;
  unsigned long elapsed = millis() - lightStartMs;

  if (elapsed < CALIBRATION_MS) {
    if (smoothed < lightRawMin) lightRawMin = smoothed;
    if (smoothed > lightRawMax) lightRawMax = smoothed;
  } else {
    // Slower drift: only adjust bounds every ~5 seconds
    static unsigned long lastDrift = 0;
    if (smoothed < lightRawMin) {
      lightRawMin = smoothed;
    } else if (millis() - lastDrift > 5000 && smoothed - lightRawMin > 100) {
      lightRawMin += 1;
      lastDrift = millis();
    }
    if (smoothed > lightRawMax) {
      lightRawMax = smoothed;
    } else if (millis() - lastDrift > 5000 && lightRawMax - smoothed > 100) {
      lightRawMax -= 1;
      lastDrift = millis();
    }
    calibrationDone = true;
  }

  int span = lightRawMax - lightRawMin;
  if (span < 50) span = 50;

  long scaled = (long)(smoothed - lightRawMin) * 100L / span;
  if (scaled < 0)   scaled = 0;
  if (scaled > 100) scaled = 100;
  return (int)scaled;
}

int getLightLevel() {
  int pct = computeLightPct();

  unsigned long now = millis();
  if (lastBufferPush == 0 || now - lastBufferPush >= PUSH_INTERVAL_MS) {
    lastBufferPush = now;
    lightBuffer[lightBufferHead] = pct;
    lightBufferHead = (lightBufferHead + 1) % AVG_WINDOW;
    if (lightBufferCount < AVG_WINDOW) lightBufferCount++;
  }

  int avg;
  if (lightBufferCount == 0) {
    avg = pct;
  } else {
    long sum = 0;
    for (int i = 0; i < lightBufferCount; i++) sum += lightBuffer[i];
    avg = (int)(sum / lightBufferCount);
  }

  // Deadband: only update the reported value if the change is meaningful.
  // This prevents constant ±1% flicker on the dashboard while still
  // responding promptly to real light changes.
  if (lastReportedPct < 0 || abs(avg - lastReportedPct) >= DEADBAND_PCT) {
    lastReportedPct = avg;
  }
  return lastReportedPct;
}

bool recalibrateLight() {
  lightRawMin = (int)lightSmoothed;
  lightRawMax = (int)lightSmoothed + 1;
  lightStartMs = millis();
  calibrationDone = false;

  lightBufferCount = 0;
  lightBufferHead  = 0;
  lastBufferPush   = 0;
  lastReportedPct  = -1;
  return true;
}

bool isLightCalibrated() {
  return calibrationDone;
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
