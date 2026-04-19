#include <Wire.h>
#include <IskakINO_LiquidCrystal_I2C.h>
#include "config.h"

LiquidCrystal_I2C lcd(16, 2);
static bool lcdReady = false;

static char lastLine1[17] = "";
static char lastLine2[17] = "";

// ---- Presence-triggered display state ----
static const unsigned long LCD_ON_DURATION_MS = 10000;   // 10 seconds
static unsigned long lastPresenceMs = 0;
static bool          lcdIsOn        = false;

// Forward declarations — implemented in profile.ino
extern bool        isProfileLoaded();
extern const char* getNickname();
extern float       getTempMin();
extern float       getTempMax();
extern float       getHumMin();
extern float       getHumMax();
extern int         getLightMin();
extern int         getLightMax();

void initLCD() {
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcdReady = true;

  lcd.setCursor(0, 0);
  lcd.print("Plant waking up");
  lcd.setCursor(0, 1);
  lcd.print("...");

  // Start "on" so you can confirm it's alive at boot.
  // Will time out 10s later and stay off until presence is seen.
  lastPresenceMs = millis();
  lcdIsOn = true;
}

static void lcdWriteLine(int row, const char* text) {
  if (!lcdReady) return;
  char padded[17];
  snprintf(padded, sizeof(padded), "%-16s", text);
  lcd.setCursor(0, row);
  lcd.print(padded);
}

// Pick a short status message using the active plant profile's ranges.
static const char* pickStatus(float tempC, float humidity, int light) {
  if (tempC > getTempMax())           return "Too hot for me!";
  if (tempC < getTempMin())           return "Brr, too cold";
  if (humidity < getHumMin())         return "Thirsty... dry";
  if (humidity > getHumMax())         return "Too humid!";
  if (light < getLightMin())          return "More light pls";
  if (light > getLightMax())          return "Too bright!";
  return isProfileLoaded() ? "I'm comfy :)" : "Setup me :)";
}

// Called once per second from sketch.ino with the current sensor readings.
// Presence is passed in (sourced from the VL53L4CD distance sensor).
void updateLCD(float tempC, float humidity, int light, bool presence) {
  if (!lcdReady) return;

  unsigned long now = millis();

  // Refresh the "last seen" timestamp every time presence is true
  if (presence) {
    lastPresenceMs = now;
  }

  // Decide whether the LCD should currently be on
  bool shouldBeOn = (now - lastPresenceMs) < LCD_ON_DURATION_MS;

  // --- Transitioning off → on ---
  if (shouldBeOn && !lcdIsOn) {
    lcd.backlight();
    // Force a redraw by clearing the "last line" cache
    lastLine1[0] = '\0';
    lastLine2[0] = '\0';
    lcdIsOn = true;
  }

  // --- Transitioning on → off ---
  if (!shouldBeOn && lcdIsOn) {
    lcd.clear();
    lcd.noBacklight();
    lastLine1[0] = '\0';
    lastLine2[0] = '\0';
    lcdIsOn = false;
    return;  // nothing more to draw
  }

  // If we're off, skip drawing
  if (!lcdIsOn) return;

  // --- Normal drawing while on ---
  const char* line1;
  // For the first 2 seconds after presence detected, show a greeting
  if (now - lastPresenceMs < 2000) {
    line1 = "Hi there! :)";
  } else {
    line1 = pickStatus(tempC, humidity, light);
  }

 // Map light % to a short tier label that fits on the LCD
  const char* lightTier;
  if      (light <= 20) lightTier = "Dark";
  else if (light <= 40) lightTier = "Dim";
  else if (light <= 60) lightTier = "Mod";
  else if (light <= 80) lightTier = "Bright";
  else                  lightTier = "Full";

  char line2[17];
  snprintf(line2, sizeof(line2), "T:%d H:%d %s",
           (int)tempC, (int)humidity, lightTier);

  if (strcmp(line1, lastLine1) != 0) {
    lcdWriteLine(0, line1);
    strncpy(lastLine1, line1, sizeof(lastLine1));
    lastLine1[sizeof(lastLine1) - 1] = '\0';
  }
  if (strcmp(line2, lastLine2) != 0) {
    lcdWriteLine(1, line2);
    strncpy(lastLine2, line2, sizeof(lastLine2));
    lastLine2[sizeof(lastLine2) - 1] = '\0';
  }
}
