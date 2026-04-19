#include <Wire.h>
#include <IskakINO_LiquidCrystal_I2C.h>
#include "config.h"

LiquidCrystal_I2C lcd(16, 2);
static bool lcdReady = false;

static unsigned long lastEventDisplayTime = 0;
static int currentEventCode = 0;
static const unsigned long EVENT_DISPLAY_MS = 5000;

static char lastLine1[17] = "";
static char lastLine2[17] = "";

void initLCD() {
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcdReady = true;

  lcd.setCursor(0, 0);
  lcd.print("Plant waking up");
  lcd.setCursor(0, 1);
  lcd.print("...");
}

static void lcdWriteLine(int row, const char* text) {
  if (!lcdReady) return;
  char padded[17];
  snprintf(padded, sizeof(padded), "%-16s", text);
  lcd.setCursor(0, row);
  lcd.print(padded);
}

void updateLCD(float tempC, float humidity, int light, bool presence, int eventCode) {
  if (!lcdReady) return;

  if (eventCode != 0) {
    currentEventCode = eventCode;
    lastEventDisplayTime = millis();
  }

  const char* line1 = "I'm comfy :)";

  bool showingEvent = (currentEventCode != 0) &&
                      (millis() - lastEventDisplayTime < EVENT_DISPLAY_MS);

  if (showingEvent) {
    switch (currentEventCode) {
      case 1: line1 = "Wheee! Moving!"; break;
      case 2: line1 = "Ouch! :("; break;
      case 3: line1 = "HELP! Tipping!"; break;
    }
  } else {
    currentEventCode = 0;
    if (presence)             line1 = "Hi there! :)";
    else if (tempC > 28)      line1 = "Too hot for me!";
    else if (tempC < 16)      line1 = "Brr, too cold";
    else if (humidity < 35)   line1 = "Thirsty... dry";
    else if (humidity > 75)   line1 = "Too humid!";
    else if (light < 20)      line1 = "More light pls";
    else if (light > 90)      line1 = "Too bright!";
    else                      line1 = "I'm comfy :)";
  }

  char line2[17];
  snprintf(line2, sizeof(line2), "%dC %d%% L%d",
           (int)tempC, (int)humidity, light);

  if (strcmp(line1, lastLine1) != 0) {
    lcdWriteLine(0, line1);
    strncpy(lastLine1, line1, sizeof(lastLine1));
  }
  if (strcmp(line2, lastLine2) != 0) {
    lcdWriteLine(1, line2);
    strncpy(lastLine2, line2, sizeof(lastLine2));
  }
}
