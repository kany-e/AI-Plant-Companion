import json
import math
import os
import time
import traceback

from dotenv import load_dotenv
load_dotenv()

from arduino.app_utils import App, Bridge
from arduino.app_bricks.web_ui import WebUI

# FastAPI is what powers the WebUI brick under the hood.
# We need Body() to accept a JSON body on POST endpoints.
from fastapi import Body
from typing import Optional

import plantbrain

print("=== AI Plant Companion starting ===")

ui = WebUI()

# ---------------------------------------------------------------------------
# Persistence
# ---------------------------------------------------------------------------
PLANT_FILE = "/tmp/plant.json"

def load_plant():
    try:
        with open(PLANT_FILE, "r") as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {
            "setup_complete": False,
            "species": None,
            "common_name": None,
            "nickname": None,
            "profile": None,
            "last_diagnostic": None,
        }

def save_plant():
    try:
        with open(PLANT_FILE, "w") as f:
            json.dump(plant, f)
    except Exception as e:
        print(f"Could not save plant file: {e}")

plant = load_plant()

# ---------------------------------------------------------------------------
# Live state
# ---------------------------------------------------------------------------
state = {
    "temperature_c": None,
    "temperature_f": None,
    "humidity": None,
    "light_level": None,
    "distance_mm": None,
    "presence": False,
    "mood": None,
    "prompts": [],
    "issues": [],
    "updated": None,
    "history": [],
    "arduino_synced": False,
}

# ---------------------------------------------------------------------------
# Push profile to Arduino
# ---------------------------------------------------------------------------
def push_profile_to_arduino():
    if not plant.get("setup_complete") or not plant.get("profile"):
        try:
            Bridge.call("clearPlantProfile")
            state["arduino_synced"] = False
        except Exception:
            pass
        return False

    profile = plant["profile"]
    t = profile.get("temp_c", {}) or {}
    h = profile.get("humidity", {}) or {}
    l = profile.get("light_pct", {}) or {}

    nickname = (plant.get("nickname") or "Plant")[:16]
    try:
        ok = Bridge.call(
            "setPlantProfile",
            nickname,
            float(t.get("min", 16.0)),
            float(t.get("max", 28.0)),
            float(h.get("min", 35.0)),
            float(h.get("max", 75.0)),
            int(l.get("min", 20)),
            int(l.get("max", 90)),
        )
        state["arduino_synced"] = bool(ok)
        if ok:
            print(f"Pushed profile to Arduino: {nickname} "
                  f"T:{t.get('min')}-{t.get('max')} "
                  f"H:{h.get('min')}-{h.get('max')} "
                  f"L:{l.get('min')}-{l.get('max')}")
        return bool(ok)
    except Exception as e:
        print(f"Could not push profile to Arduino: {e}")
        state["arduino_synced"] = False
        return False

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
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

# ---------------------------------------------------------------------------
# API endpoints — clean FastAPI signatures
# ---------------------------------------------------------------------------

def api_readings():
    """GET /api/readings — polled every second by the frontend."""
    return sanitize({
        "state": state,
        "plant": {
            "setup_complete": plant.get("setup_complete", False),
            "species": plant.get("species"),
            "common_name": plant.get("common_name"),
            "nickname": plant.get("nickname"),
        },
    })

def api_plant():
    """GET /api/plant — full plant profile + last diagnostic."""
    return sanitize({
        "plant": {
            "setup_complete": plant.get("setup_complete", False),
            "species": plant.get("species"),
            "common_name": plant.get("common_name"),
            "nickname": plant.get("nickname"),
        },
        "profile": plant.get("profile"),
        "last_diagnostic": plant.get("last_diagnostic"),
    })

def api_identify(payload: dict = Body(...)):
    """POST /api/identify — body: {image: b64, media_type: str}"""
    print(f">>> api_identify called with payload keys: {list(payload.keys()) if isinstance(payload, dict) else type(payload)}")
    try:
        image = payload.get("image")
        media_type = payload.get("media_type") or "image/jpeg"
        if not image:
            return {"ok": False, "error": "missing image"}
        result = plantbrain.identify_plant(image, media_type=media_type)
        return {"ok": True, "result": result}
    except Exception as e:
        traceback.print_exc()
        return {"ok": False, "error": str(e)}

def api_select_plant(payload: dict = Body(...)):
    """POST /api/select_plant — body: {species, common_name?, nickname?}"""
    print(f">>> api_select_plant called with payload: {payload}")
    try:
        species = (payload.get("species") or "").strip()
        common_name = (payload.get("common_name") or "").strip() or None
        nickname = (payload.get("nickname") or "").strip() or (common_name or species)
        if not species:
            return {"ok": False, "error": "species is required"}

        profile = plantbrain.research_plant(species, common_name=common_name)

        plant["species"] = species
        plant["common_name"] = common_name or profile.get("common_name")
        plant["nickname"] = nickname
        plant["profile"] = profile
        plant["setup_complete"] = True
        save_plant()

        push_profile_to_arduino()

        return {
            "ok": True,
            "plant": {
                "species": plant["species"],
                "common_name": plant["common_name"],
                "nickname": plant["nickname"],
                "profile": profile,
            },
        }
    except Exception as e:
        traceback.print_exc()
        return {"ok": False, "error": str(e)}

def api_diagnose(payload: dict = Body(...)):
    """POST /api/diagnose — body: {image: b64, media_type: str}"""
    print(f">>> api_diagnose called")
    try:
        image = payload.get("image")
        media_type = payload.get("media_type") or "image/jpeg"
        if not image:
            return {"ok": False, "error": "missing image"}
        result = plantbrain.diagnose_plant(
            image, species=plant.get("species"), media_type=media_type
        )
        plant["last_diagnostic"] = {
            "result": result,
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        }
        save_plant()
        return {"ok": True, "result": result}
    except Exception as e:
        traceback.print_exc()
        return {"ok": False, "error": str(e)}

def api_reset_plant():
    """POST /api/reset_plant — clears the saved plant."""
    print(">>> api_reset_plant called")
    plant.update({
        "setup_complete": False,
        "species": None,
        "common_name": None,
        "nickname": None,
        "profile": None,
        "last_diagnostic": None,
    })
    try:
        os.remove(PLANT_FILE)
    except FileNotFoundError:
        pass
    try:
        Bridge.call("clearPlantProfile")
    except Exception:
        pass
    state["arduino_synced"] = False
    return {"ok": True}

ui.expose_api("GET",  "/api/readings",     api_readings)
ui.expose_api("GET",  "/api/plant",        api_plant)
ui.expose_api("POST", "/api/identify",     api_identify)
ui.expose_api("POST", "/api/select_plant", api_select_plant)
ui.expose_api("POST", "/api/diagnose",     api_diagnose)
ui.expose_api("POST", "/api/reset_plant",  api_reset_plant)

# Push profile on startup so a fresh boot catches up immediately
push_profile_to_arduino()

# ---------------------------------------------------------------------------
# Main loop — runs every ~1s
# ---------------------------------------------------------------------------
def loop():
    try:
        if plant.get("setup_complete") and not state["arduino_synced"]:
            push_profile_to_arduino()
        try:
            arduino_has_it = bool(Bridge.call("hasProfile"))
            if plant.get("setup_complete") and not arduino_has_it:
                push_profile_to_arduino()
        except Exception:
            pass

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

        if plant.get("setup_complete") and plant.get("profile"):
            eval_result = plantbrain.evaluate_environment(
                state,
                plant["profile"],
                nickname=plant.get("nickname") or "I",
            )
            state["mood"]    = eval_result["mood"]
            state["issues"]  = eval_result["issues"]
            state["prompts"] = eval_result["prompts"]
        else:
            state["mood"]    = None
            state["issues"]  = []
            state["prompts"] = []

        state["history"].append({
            "t": state["updated"],
            "temp": temp_c_clean,
            "hum": state["humidity"],
            "light": state["light_level"],
        })
        state["history"] = state["history"][-60:]

        mood_tag = {"happy": "😊", "uncomfortable": "😟", "alarmed": "🚨"}.get(state["mood"], "  ")
        presence_tag = "P" if state["presence"] else " "
        sync_tag = "✓" if state["arduino_synced"] else "…"
        dist_str = "----" if state["distance_mm"] is None else f"{state['distance_mm']:4d}"
        t_str    = "--"   if temp_c_clean is None else f"{temp_c_clean:.1f}"
        h_str    = "--"   if state["humidity"] is None else f"{state['humidity']:.1f}"
        print(f"{mood_tag} {presence_tag} {sync_tag} T={t_str}C  H={h_str}%  L={state['light_level']}%  D={dist_str}mm")

    except Exception as e:
        print(f"Loop error: {e}")

    time.sleep(1)

App.run(user_loop=loop)
