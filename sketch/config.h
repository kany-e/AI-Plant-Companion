#pragma once

// ---- Pins ----
const int LIGHT_PIN = A1;

// ---- LED matrix geometry ----
const int ROWS = 8;
const int COLS = 13;

// ---- Presence detection ----
const int PRESENCE_THRESHOLD_MM = 500;   // 50 cm

// ---- Movement detection thresholds ----
const float TILT_THRESHOLD_DEG    = 20.0;
const float BUMP_THRESHOLD        = 1.8;    // g
const float MOTION_THRESHOLD      = 0.25;   // g delta from baseline
const unsigned long ALERT_COOLDOWN_MS = 1500;

// ---- Event codes (shared with Python) ----
// 0 = nothing, 1 = movement, 2 = bump, 3 = tipping