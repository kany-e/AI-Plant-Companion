import os
import json
import urllib.request
import urllib.error

API_URL = "https://api.anthropic.com/v1/messages"
VISION_MODEL = "claude-opus-4-7"       # best vision + reasoning
TEXT_MODEL   = "claude-sonnet-4-6"     # fast + cheap for research / eval

# ---------------------------------------------------------------------------
# Low-level API helper
# ---------------------------------------------------------------------------

def _call_claude(model, messages, max_tokens=1024, system=None):
    """Minimal Anthropic API client using stdlib only (no `requests` needed).

    Returns the assistant text content, or raises RuntimeError on failure.
    """
    api_key = os.environ.get("ANTHROPIC_API_KEY")
    if not api_key:
        raise RuntimeError("ANTHROPIC_API_KEY environment variable is not set")

    body = {
        "model": model,
        "max_tokens": max_tokens,
        "messages": messages,
    }
    if system:
        body["system"] = system

    req = urllib.request.Request(
        API_URL,
        data=json.dumps(body).encode("utf-8"),
        headers={
            "Content-Type": "application/json",
            "x-api-key": api_key,
            "anthropic-version": "2023-06-01",
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=60) as resp:
            data = json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        err_body = e.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"Claude API HTTP {e.code}: {err_body}") from e
    except Exception as e:
        raise RuntimeError(f"Claude API call failed: {e}") from e

    # data["content"] is a list of blocks; concatenate text blocks.
    parts = []
    for block in data.get("content", []):
        if block.get("type") == "text":
            parts.append(block.get("text", ""))
    return "\n".join(parts).strip()


def _extract_json(text):
    """Strip markdown fences and parse JSON out of a model response."""
    t = text.strip()
    if t.startswith("```"):
        # drop opening fence line and optional trailing fence
        lines = t.splitlines()
        if lines and lines[0].startswith("```"):
            lines = lines[1:]
        if lines and lines[-1].startswith("```"):
            lines = lines[:-1]
        t = "\n".join(lines).strip()
    return json.loads(t)


# ---------------------------------------------------------------------------
# 1. Plant identification from photo
# ---------------------------------------------------------------------------

def identify_plant(image_b64, media_type="image/jpeg"):
    """Given a base64-encoded image, return a species guess.

    Returns: {"species": str, "common_name": str, "confidence": "high|medium|low", "notes": str}
    """
    system = (
        "You are a botanist identifying houseplants from photos. "
        "Respond ONLY with valid JSON matching the schema the user requests. "
        "No markdown, no prose outside the JSON."
    )
    prompt = (
        "Identify the houseplant in this photo. Respond with JSON only:\n"
        "{\n"
        '  "species": "scientific name or best guess",\n'
        '  "common_name": "common English name",\n'
        '  "confidence": "high" | "medium" | "low",\n'
        '  "notes": "one short sentence explaining the call"\n'
        "}\n"
        "If no plant is visible, set species to \"unknown\" and confidence to \"low\"."
    )
    messages = [{
        "role": "user",
        "content": [
            {"type": "image",
             "source": {"type": "base64", "media_type": media_type, "data": image_b64}},
            {"type": "text", "text": prompt},
        ],
    }]
    text = _call_claude(VISION_MODEL, messages, max_tokens=400, system=system)
    return _extract_json(text)


# ---------------------------------------------------------------------------
# 2. Care profile research (cached per species by the caller)
# ---------------------------------------------------------------------------

def research_plant(species, common_name=None):
    """Return a structured care profile for the given plant species.

    Returns a dict with ideal ranges the environment evaluator can use directly.
    """
    label = f"{common_name} ({species})" if common_name else species
    system = (
        "You are a houseplant care expert. Respond with JSON only, no markdown, "
        "no prose. All numeric ranges must be realistic for indoor cultivation."
    )
    prompt = (
        f"Provide a care profile for: {label}.\n\n"
        "Respond with JSON only, exactly matching this schema:\n"
        "{\n"
        '  "species": "<species>",\n'
        '  "common_name": "<common name>",\n'
        '  "temp_c":   {"min": <number>, "max": <number>, "ideal": <number>},\n'
        '  "humidity": {"min": <number>, "max": <number>, "ideal": <number>},\n'
        '  "light_pct": {"min": <number>, "max": <number>, "ideal": <number>},\n'
        '  "watering":   "<one-sentence watering guidance>",\n'
        '  "light_note": "<one-sentence light placement guidance>",\n'
        '  "fun_fact":   "<one short fun fact about this plant>",\n'
        '  "persona":    "<two adjectives describing its vibe, e.g. chill, dramatic>"\n'
        "}\n"
        "humidity and light_pct values are percentages from 0 to 100. "
        "temp_c values are in Celsius."
    )
    messages = [{"role": "user", "content": prompt}]
    text = _call_claude(TEXT_MODEL, messages, max_tokens=600, system=system)
    return _extract_json(text)


# ---------------------------------------------------------------------------
# 3. Photo-based health diagnostic
# ---------------------------------------------------------------------------

def diagnose_plant(image_b64, species=None, media_type="image/jpeg"):
    """Examine a plant photo and return a health report.

    Returns: {
      "overall_health": "healthy|mild|serious",
      "issues":      [{"name": str, "evidence": str, "severity": "low|medium|high"}],
      "treatments":  [str, ...],
      "summary":     "one short paragraph"
    }
    """
    context = f" The plant is a {species}." if species else ""
    system = (
        "You are a plant-health specialist diagnosing houseplants from photos. "
        "Be specific about visible symptoms (leaf color, spots, droop, pests, soil). "
        "Respond with JSON only, no markdown, no prose outside the JSON."
    )
    prompt = (
        f"Examine this plant photo and report visible health issues.{context}\n\n"
        "Respond with JSON only, matching this schema:\n"
        "{\n"
        '  "overall_health": "healthy" | "mild" | "serious",\n'
        '  "issues": [\n'
        '    {"name": "<short name>", "evidence": "<what you see>", "severity": "low"|"medium"|"high"}\n'
        "  ],\n"
        '  "treatments": ["<actionable step 1>", "<actionable step 2>", ...],\n'
        '  "summary": "<two sentences max, plain language>"\n'
        "}\n"
        "If the plant looks healthy, return an empty issues array and a short encouraging summary."
    )
    messages = [{
        "role": "user",
        "content": [
            {"type": "image",
             "source": {"type": "base64", "media_type": media_type, "data": image_b64}},
            {"type": "text", "text": prompt},
        ],
    }]
    text = _call_claude(VISION_MODEL, messages, max_tokens=800, system=system)
    return _extract_json(text)


# ---------------------------------------------------------------------------
# 4. Live environment evaluation (rule-based, NO API CALL)
# ---------------------------------------------------------------------------

def evaluate_environment(state, profile, nickname="I"):
    """Compare live sensor readings against the plant's profile.

    Pure Python, runs every loop tick. No API call. Returns:
        {
          "mood":   "happy" | "uncomfortable" | "alarmed",
          "issues": ["too_hot", "too_dry", ...],
          "prompts": ["I'm too hot right now", ...]
        }
    """
    issues, prompts = [], []

    if not profile:
        return {"mood": "unknown", "issues": [], "prompts": [f"{nickname} hasn't been set up yet."]}

    temp = state.get("temperature_c")
    hum  = state.get("humidity")
    lux  = state.get("light_level")

    # Temperature
    t_profile = profile.get("temp_c", {})
    if temp is not None and t_profile:
        if temp > t_profile.get("max", 99):
            issues.append("too_hot")
            prompts.append(f"It's too hot here — {temp:.1f}°C is above my comfort range.")
        elif temp < t_profile.get("min", -99):
            issues.append("too_cold")
            prompts.append(f"Brrr, {temp:.1f}°C is too cold for me.")

    # Humidity
    h_profile = profile.get("humidity", {})
    if hum is not None and h_profile:
        if hum < h_profile.get("min", 0):
            issues.append("too_dry")
            prompts.append(f"The air is too dry ({hum:.0f}%) — I could use a mist.")
        elif hum > h_profile.get("max", 100):
            issues.append("too_humid")
            prompts.append(f"It's very humid ({hum:.0f}%). Watch for mold on my leaves.")

    # Light
    l_profile = profile.get("light_pct", {})
    if lux is not None and l_profile:
        if lux < l_profile.get("min", 0):
            issues.append("too_dark")
            prompts.append("It's pretty dark where I am. Can you move me closer to a window?")
        elif lux > l_profile.get("max", 100):
            issues.append("too_bright")
            prompts.append("The light is intense here — I might get scorched.")

    # Event-based prompts (movement/bump/tipping)
    last_event = state.get("last_event")
    if last_event == "tipping":
        issues.append("tipping")
        prompts.append("Whoa — I'm tipping over! Please set me upright.")
    elif last_event == "bump":
        issues.append("bump")
        prompts.append("Ouch, something bumped into me.")
    elif last_event == "movement":
        prompts.append("I was just moved to a new spot.")

    # Overall mood
    severe = {"too_hot", "too_cold", "tipping"}
    if any(i in severe for i in issues):
        mood = "alarmed"
    elif issues:
        mood = "uncomfortable"
    else:
        mood = "happy"
        prompts.append(f"I'm comfortable right now. Thanks for taking care of me!")

    return {"mood": mood, "issues": issues, "prompts": prompts}
