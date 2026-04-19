#include <math.h>
#include "config.h"

extern ModulinoMovement movement;
extern int lastMovementEvent;

static float baseX = 0, baseY = 0, baseZ = 1.0;
static bool  baselineReady = false;
static unsigned long lastAlertTime = 0;

void captureMovementBaseline() {
  float sumX = 0, sumY = 0, sumZ = 0;
  int samples = 0;
  unsigned long start = millis();
  while (millis() - start < 1500) {
    if (movement.available()) {
      sumX += movement.getX();
      sumY += movement.getY();
      sumZ += movement.getZ();
      samples++;
    }
    delay(40);
  }
  if (samples > 0) {
    baseX = sumX / samples;
    baseY = sumY / samples;
    baseZ = sumZ / samples;
    baselineReady = true;
  }
}

void initMovement() {
  delay(500);
  captureMovementBaseline();
}

// Exposed to Python so user can recalibrate from dashboard
void recalibrateBaseline() {
  captureMovementBaseline();
}

bool isBaselineReady() {
  return baselineReady;
}

int getMovementEvent() {
  int e = lastMovementEvent;
  lastMovementEvent = 0;  // one-shot read
  return e;
}

void updateMovement() {
  if (!movement.available() || !baselineReady) return;
  if (millis() - lastAlertTime < ALERT_COOLDOWN_MS) return;

  float ax = movement.getX();
  float ay = movement.getY();
  float az = movement.getZ();

  float dx = ax - baseX, dy = ay - baseY, dz = az - baseZ;
  float motionDelta = sqrt(dx*dx + dy*dy + dz*dz);

  float dot = ax*baseX + ay*baseY + az*baseZ;
  float magNow  = sqrt(ax*ax + ay*ay + az*az);
  float magBase = sqrt(baseX*baseX + baseY*baseY + baseZ*baseZ);
  float tiltDeg = 0;
  if (magNow > 0.01 && magBase > 0.01) {
    float cosA = dot / (magNow * magBase);
    if (cosA > 1)  cosA = 1;
    if (cosA < -1) cosA = -1;
    tiltDeg = acos(cosA) * 180.0 / PI;
  }

  int newEvent = 0;
  if (tiltDeg > TILT_THRESHOLD_DEG)        newEvent = 3; // TIPPING
  else if (magNow > BUMP_THRESHOLD)        newEvent = 2; // BUMP
  else if (motionDelta > MOTION_THRESHOLD) newEvent = 1; // MOVEMENT

  if (newEvent != 0) {
    lastMovementEvent = newEvent;
    lastAlertTime = millis();
  }
}