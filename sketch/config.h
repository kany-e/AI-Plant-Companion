#pragma once

// ---- Pins ----
const int LIGHT_PIN = A1;

// ---- LED matrix geometry ----
const int ROWS = 8;
const int COLS = 13;

// ---- Presence detection ----
const int PRESENCE_THRESHOLD_MM = 500;   // 50 cm

// ---- Fallback thresholds used by the LCD when no plant profile has been
//      pushed from Python yet (fresh boot, Python not running, etc.).
//      Once Python calls setPlantProfile(...), these are replaced in RAM
//      by the plant-specific ranges.
const float DEFAULT_TEMP_MIN   = 16.0f;
const float DEFAULT_TEMP_MAX   = 28.0f;
const float DEFAULT_HUM_MIN    = 35.0f;
const float DEFAULT_HUM_MAX    = 75.0f;
const int   DEFAULT_LIGHT_MIN  = 20;
const int   DEFAULT_LIGHT_MAX  = 90;
