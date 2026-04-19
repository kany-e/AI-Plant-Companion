// Forward declarations (functions defined elsewhere)
float getTempC();
float getHumidity();
int   getLightLevel();
int   getDistanceMm();
bool  getPresence();
bool  recalibrateLight();
bool  isLightCalibrated();

// From profile.ino — note: String, not const char*, to satisfy MsgPack
bool  setPlantProfile(String nickname,
                      float tMin, float tMax,
                      float hMin, float hMax,
                      int   lMin, int   lMax);
bool  clearPlantProfile();
bool  hasProfile();


void registerBridgeAPI() {
  // Sensor readings (Python → Arduino getters)
  Bridge.provide("getTempC",          getTempC);
  Bridge.provide("getHumidity",       getHumidity);
  Bridge.provide("getLightLevel",     getLightLevel);
  Bridge.provide("getDistanceMm",     getDistanceMm);
  Bridge.provide("getPresence",       getPresence);

  // Light calibration controls
  Bridge.provide("recalibrateLight",  recalibrateLight);
  Bridge.provide("isLightCalibrated", isLightCalibrated);

  // Plant profile (Python pushes to Arduino)
  Bridge.provide("setPlantProfile",   setPlantProfile);
  Bridge.provide("clearPlantProfile", clearPlantProfile);
  Bridge.provide("hasProfile",        hasProfile);
}
