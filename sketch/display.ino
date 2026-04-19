#include "config.h"

extern Arduino_LED_Matrix matrix;
extern void updateMovement();

// 5x7 font
static const uint8_t FONT[][5] = {
  {0x00,0x00,0x00,0x00,0x00}, // space
  {0x3E,0x51,0x49,0x45,0x3E}, // 0
  {0x00,0x42,0x7F,0x40,0x00}, // 1
  {0x42,0x61,0x51,0x49,0x46}, // 2
  {0x21,0x41,0x45,0x4B,0x31}, // 3
  {0x18,0x14,0x12,0x7F,0x10}, // 4
  {0x27,0x45,0x45,0x45,0x39}, // 5
  {0x3C,0x4A,0x49,0x49,0x30}, // 6
  {0x01,0x71,0x09,0x05,0x03}, // 7
  {0x36,0x49,0x49,0x49,0x36}, // 8
  {0x06,0x49,0x49,0x29,0x1E}, // 9
  {0x3E,0x41,0x41,0x41,0x22}, // C (11)
  {0x7F,0x08,0x08,0x08,0x7F}, // H (12)
  {0x23,0x13,0x08,0x64,0x62}, // %
  {0x00,0x60,0x60,0x00,0x00}, // .
  {0x7F,0x40,0x40,0x40,0x40}, // L (15)
};

static int charIdx(char c) {
  if (c == ' ') return 0;
  if (c >= '0' && c <= '9') return 1 + (c - '0');
  if (c == 'C') return 11;
  if (c == 'H') return 12;
  if (c == '%') return 13;
  if (c == '.') return 14;
  if (c == 'L') return 15;
  return 0;
}

static int buildColumns(const char* msg, uint8_t* cols, int maxCols) {
  int n = 0;
  for (int i = 0; i < COLS; i++) cols[n++] = 0;
  for (int i = 0; msg[i] && n < maxCols - 7; i++) {
    int idx = charIdx(msg[i]);
    for (int c = 0; c < 5; c++) cols[n++] = FONT[idx][c];
    cols[n++] = 0;
  }
  for (int i = 0; i < COLS; i++) cols[n++] = 0;
  return n;
}

static void packFrame(const uint8_t* cols, int offset, uint32_t* frame) {
  frame[0] = frame[1] = frame[2] = frame[3] = 0;
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      uint8_t colByte = cols[offset + c];
      if (colByte & (1 << r)) {
        int bitIdx = r * COLS + c;
        frame[bitIdx / 32] |= (1UL << (31 - (bitIdx % 32)));
      }
    }
  }
}

void scrollMessage(const char* msg, int frameDelay) {
  uint8_t cols[256];
  int total = buildColumns(msg, cols, 256);
  uint32_t frame[4];
  for (int offset = 0; offset <= total - COLS; offset++) {
    packFrame(cols, offset, frame);
    matrix.loadFrame(frame);
    // Keep polling the IMU during the frame delay so movement events
    // aren't missed while the matrix is scrolling.
    unsigned long t0 = millis();
    while (millis() - t0 < (unsigned long)frameDelay) {
      updateMovement();
      delay(2);
    }
  }
}

void initDisplay() {
  matrix.begin();
  matrix.clear();
}