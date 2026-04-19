// Forward declarations (functions defined elsewhere)
float getTempC();
float getHumidity();
int   getLightLevel();
int   getDistanceMm();
bool  getPresence();

void registerBridgeAPI() {
  Bridge.provide("getTempC",      getTempC);
  Bridge.provide("getHumidity",   getHumidity);
  Bridge.provide("getLightLevel", getLightLevel);
  Bridge.provide("getDistanceMm", getDistanceMm);
  Bridge.provide("getPresence",   getPresence);
}
