import math
import time
from arduino.app_utils import App, Bridge
from arduino.app_bricks.web_ui import WebUI

print("=== AI Plant Companion starting ===")

ui = WebUI()

state = {
    "temperature_c": None,
    "temperature_f": None,
    "humidity": None,
    "light_level": None,
    "distance_mm": None,
    "presence": False,
    "updated": None,
    "history": [],
}

def safe_float(v, ndigits=2):
    if v is None:
        return None
    try:
        f = float(v)
    except (TypeError, ValueError):
        return None
    if not math.isfinite(f):
        return None
    return round(f, ndigits)

def sanitize(obj):
    if isinstance(obj, float):
        return obj if math.isfinite(obj) else None
    if isinstance(obj, dict):
        return {k: sanitize(v) for k, v in obj.items()}
    if isinstance(obj, list):
        return [sanitize(v) for v in obj]
    return obj

def get_readings():
    return sanitize(state)

ui.expose_api("GET", "/api/readings", get_readings)

def loop():
    try:
        temp_c   = Bridge.call("getTempC")
        humidity = Bridge.call("getHumidity")
        light    = Bridge.call("getLightLevel")
        dist_mm  = Bridge.call("getDistanceMm")
        presence = Bridge.call("getPresence")

        temp_c_clean = safe_float(temp_c)
        state["temperature_c"] = temp_c_clean
        state["temperature_f"] = safe_float((temp_c_clean * 9 / 5) + 32) if temp_c_clean is not None else None
        state["humidity"]      = safe_float(humidity)
        state["light_level"]   = light if isinstance(light, int) else None
        state["distance_mm"]   = dist_mm if isinstance(dist_mm, int) and dist_mm != 9999 else None
        state["presence"]      = bool(presence)
        state["updated"]       = time.strftime("%H:%M:%S")

        state["history"].append({
            "t": state["updated"],
            "temp": temp_c_clean,
            "hum": state["humidity"],
            "light": state["light_level"],
        })
        state["history"] = state["history"][-60:]

        presence_tag = "P" if state["presence"] else " "
        dist_str = "----" if state["distance_mm"] is None else f"{state['distance_mm']:4d}"
        t_str    = "--"   if temp_c_clean is None else f"{temp_c_clean:.1f}"
        h_str    = "--"   if state["humidity"] is None else f"{state['humidity']:.1f}"
        print(f"{presence_tag} T={t_str}C  H={h_str}%  L={state['light_level']}%  D={dist_str}mm")
    except Exception as e:
        print(f"Error: {e}")

    time.sleep(1)

App.run(user_loop=loop)
