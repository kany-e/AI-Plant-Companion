// Forward declarations (functions defined elsewhere)
float getTempC();
float getHumidity();
int   getLightLevel();
int   getDistanceMm();
bool  getPresence();
int   getMovementEvent();
void  recalibrateBaseline();
bool  isBaselineReady();
float getMotionDelta();
float getTiltDeg();
float getMagNow();

void registerBridgeAPI() {
  Bridge.provide("getTempC",             getTempC);
  Bridge.provide("getHumidity",          getHumidity);
  Bridge.provide("getLightLevel",        getLightLevel);
  Bridge.provide("getDistanceMm",        getDistanceMm);
  Bridge.provide("getPresence",          getPresence);
  Bridge.provide("getMovementEvent",     getMovementEvent);
  Bridge.provide("recalibrateBaseline",  recalibrateBaseline);
  Bridge.provide("isBaselineReady",      isBaselineReady);
  Bridge.provide("getMotionDelta",       getMotionDelta);
  Bridge.provide("getTiltDeg",           getTiltDeg);
  Bridge.provide("getMagNow",            getMagNow);
}