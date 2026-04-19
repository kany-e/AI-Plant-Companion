import time
from arduino.app_utils import App, Bridge
from arduino.app_bricks.web_ui import WebUI

print("=== AI Plant Companion starting ===")

ui = WebUI()

EVENT_NAMES = {
    0: None,
    1: "movement",
    2: "bump",
    3: "tipping",
}

state = {
    "temperature_c": None,
    "temperature_f": None,
    "humidity": None,
    "light_level": None,
    "distance_mm": None,
    "presence": False,
    "last_event": None,
    "last_event_time": None,
    "updated": None,
    "history": [],
}

def get_readings():
    return state

def recalibrate():
    Bridge.call("recalibrateBaseline")
    return {"ok": True}

ui.expose_api("GET", "/api/readings", get_readings)
ui.expose_api("POST", "/api/recalibrate", recalibrate)

def loop():
    try:
        temp_c   = Bridge.call("getTempC")
        humidity = Bridge.call("getHumidity")
        light    = Bridge.call("getLightLevel")
        dist_mm  = Bridge.call("getDistanceMm")
        presence = Bridge.call("getPresence")
        event_id = Bridge.call("getMovementEvent")

        if temp_c is not None:
            state["temperature_c"] = round(temp_c, 2)
            state["temperature_f"] = round((temp_c * 9 / 5) + 32, 2)
            state["humidity"]      = round(humidity, 2) if humidity is not None else None
            state["light_level"]   = light
            state["distance_mm"]   = dist_mm
            state["presence"]      = bool(presence)
            state["updated"]       = time.strftime("%H:%M:%S")

            name = EVENT_NAMES.get(event_id)
            if name:
                state["last_event"] = name
                state["last_event_time"] = state["updated"]
                print(f"⚠️  {name.upper()} detected at {state['updated']}")

            state["history"].append({
                "t": state["updated"],
                "temp": state["temperature_c"],
                "hum": state["humidity"],
                "light": light,
            })
            state["history"] = state["history"][-60:]

            presence_tag = "👤" if state["presence"] else "  "
            dist_str = "----" if dist_mm in (None, 9999) else f"{dist_mm:4d}"
            print(f"{presence_tag} T={temp_c:.1f}°C  H={humidity:.1f}%  L={light}%  D={dist_str}mm")
    except Exception as e:
        print(f"Error: {e}")

    time.sleep(1)

App.run(user_loop=loop)