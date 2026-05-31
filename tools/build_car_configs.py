#!/usr/bin/env python3
"""Auto-generate car.json configs from extracted WAV files.

Scans each subdirectory in cars/, identifies external engine sound layers
by filename patterns, estimates RPM from filenames, and writes car.json.

Skips directories that already have a valid car.json.

Usage:
    python3 tools/build_car_configs.py [cars_dir]
    Default: ./cars/
"""

import json
import os
import re
import sys
from pathlib import Path


# RPM keyword mapping (when no numeric RPM in filename)
RPM_KEYWORDS = {
    "idle": 900,
    "verylow": 1500,
    "low": 2500,
    "lowmid": 3500,
    "mid": 5000,
    "midhigh": 6500,
    "high": 7500,
    "veryhigh": 8500,
    "max": 9000,
}


def estimate_rpm(filename: str, is_idle: bool = False) -> int:
    """Extract or estimate RPM from a WAV filename."""
    name = Path(filename).stem.lower()

    if is_idle:
        return 900

    # Try to find 3-5 digit number that looks like RPM
    numbers = re.findall(r"(\d{3,5})", name)
    for n in numbers:
        val = int(n)
        if 500 <= val <= 15000:
            return val

    # Keyword-based
    for keyword, rpm in sorted(RPM_KEYWORDS.items(), key=lambda x: -len(x[0])):
        if keyword in name:
            return rpm

    return 0


def classify_wavs(car_dir: str) -> dict:
    """Classify WAV files in a car directory.

    Returns dict with keys: ext_on, ext_off, ext_idle, backfire
    Each is a list of (filename, rpm).
    """
    result = {"ext_on": [], "ext_off": [], "ext_idle": [], "backfire": []}

    wav_files = sorted(Path(car_dir).glob("*.wav"))

    for wav in wav_files:
        name = wav.name.lower()
        stem = wav.stem.lower()

        # Skip non-engine sounds
        if any(
            skip in name
            for skip in [
                "skid",
                "tyre",
                "wind",
                "door",
                "horn",
                "bodywork",
                "flat_tyre",
                "traction",
                "brakes",
                "rolling",
                "missgear",
                "flutter",
                "turbo",
                "limiter",
                "pop_",
                "rattle",
            ]
        ):
            continue

        # Internal (cockpit) sounds — skip
        if "_in_" in name and "_ex_" not in name and "ext_" not in name:
            continue

        # Backfire (external)
        if "backfireext" in name or ("backfire" in name and "ext" in name):
            result["backfire"].append(wav.name)
            continue

        # Gear sounds — skip (handled separately)
        if "gear" in name:
            continue

        # Generic backfire (not ext) — skip
        if "backfire" in name:
            continue

        # Identify external engine sounds
        is_ext = "_ex_" in name or "ext_" in name or "ext" in name[:4]

        # Also match patterns like "3532a.wav" (numeric RPM samples)
        is_numeric_sample = bool(re.match(r"^\d{3,5}[a-z]?(_off)?\.wav$", name))

        if not is_ext and not is_numeric_sample:
            continue

        # Classify as idle / on / off
        is_idle = "idle" in name
        is_off = "_off_" in name or "_off" in stem or "off" in name.split("_")
        is_on = "_on_" in name or "_on" in stem or "on" in name.split("_")

        # Numeric samples with _off suffix
        if is_numeric_sample and "_off" in name:
            is_off = True
        elif is_numeric_sample and "_off" not in name:
            is_on = True

        rpm = estimate_rpm(wav.name, is_idle)

        if is_idle:
            result["ext_idle"].append((wav.name, rpm if rpm > 0 else 900))
        elif is_off:
            if rpm > 0:
                result["ext_off"].append((wav.name, rpm))
        elif is_on:
            if rpm > 0:
                result["ext_on"].append((wav.name, rpm))
        elif rpm > 0:
            # Ambiguous — default to on-load
            result["ext_on"].append((wav.name, rpm))

    # Sort by RPM
    result["ext_on"].sort(key=lambda x: x[1])
    result["ext_off"].sort(key=lambda x: x[1])

    return result


def generate_car_json(car_dir: str, car_name: str) -> dict | None:
    """Generate car.json content for a car directory."""
    classified = classify_wavs(car_dir)

    # Need at least idle + 1 on-load layer
    if not classified["ext_on"] and not classified["ext_off"]:
        return None

    # Build onload layers
    onload = []
    # Add idle as first layer if available
    if classified["ext_idle"]:
        idle_file, idle_rpm = classified["ext_idle"][0]
        onload.append({"file": idle_file, "rpm": idle_rpm})

    for fname, rpm in classified["ext_on"]:
        onload.append({"file": fname, "rpm": rpm})

    # Build offload layers
    offload = []
    if classified["ext_idle"]:
        idle_file, idle_rpm = classified["ext_idle"][0]
        offload.append({"file": idle_file, "rpm": idle_rpm})

    for fname, rpm in classified["ext_off"]:
        offload.append({"file": fname, "rpm": rpm})

    # If no offload, use onload
    if not offload:
        offload = onload[:]

    # Need at least 2 layers total
    if len(onload) < 2:
        return None

    # Estimate redline from highest RPM layer
    max_rpm = max(
        (rpm for _, rpm in classified["ext_on"] + classified["ext_off"]),
        default=8000,
    )
    rpm_redline = int(max_rpm * 1.05)  # 5% above highest sample

    # Estimate idle from lowest layer
    rpm_idle = 900
    if classified["ext_idle"]:
        rpm_idle = classified["ext_idle"][0][1]
    elif onload:
        rpm_idle = onload[0]["rpm"]

    # Guess cylinder count from car name
    cylinders = guess_cylinders(car_name)

    # Pretty name
    pretty_name = car_name.replace("_", " ").replace("ks ", "").title()

    config = {
        "name": pretty_name,
        "engine": f"{cylinders} cyl",
        "cylinders": cylinders,
        "rpm_idle": rpm_idle,
        "rpm_redline": rpm_redline,
        "peak_torque": 400,
        "peak_torque_rpm": int(rpm_redline * 0.7),
        "inertia": 0.12,
        "transmission": {
            "gears": default_gears(cylinders),
            "final_drive": 3.7,
        },
        "onload": onload,
        "offload": offload,
    }

    return config


def guess_cylinders(car_name: str) -> int:
    """Guess cylinder count from car name."""
    name = car_name.lower()

    # V12 indicators
    if any(x in name for x in ["v12", "zonda", "huayra", "laferrari", "812", "599"]):
        return 12
    # V10 indicators
    if any(x in name for x in ["v10", "huracan", "gallardo", "r8"]):
        return 10
    # V8 indicators
    if any(
        x in name
        for x in [
            "v8",
            "458",
            "f40",
            "corvette",
            "cobra",
            "sls",
            "m3_e92",
            "gt2",
            "gt3",
            "mp412c",
            "1m",
            "z4",
            "yellowbird",
        ]
    ):
        return 8
    # V6 indicators
    if any(x in name for x in ["v6", "155", "giulia"]):
        return 6
    # I4 indicators
    if any(x in name for x in ["i4", "e30", "civic", "integra", "mr2", "s2000", "mito", "abarth", "500"]):
        return 4
    # Flat-6
    if any(x in name for x in ["porsche", "911", "ruf"]):
        return 6

    return 6  # Default guess


def default_gears(cylinders: int) -> list:
    """Return reasonable default gear ratios."""
    if cylinders >= 10:
        return [3.91, 2.44, 1.81, 1.40, 1.12, 0.92, 0.76]
    elif cylinders == 8:
        return [3.40, 2.14, 1.53, 1.18, 0.95, 0.79]
    elif cylinders == 6:
        return [3.50, 2.12, 1.43, 1.09, 0.86]
    else:
        return [3.54, 2.13, 1.52, 1.18, 0.97, 0.82]


def main():
    cars_dir = sys.argv[1] if len(sys.argv) > 1 else "cars"

    if not os.path.isdir(cars_dir):
        print(f"ERROR: {cars_dir} is not a directory")
        sys.exit(1)

    print(f"Scanning: {cars_dir}/\n")

    subdirs = sorted(
        [d for d in os.listdir(cars_dir) if os.path.isdir(os.path.join(cars_dir, d))]
    )

    created = 0
    skipped_existing = 0
    skipped_no_data = 0

    for car_name in subdirs:
        car_dir = os.path.join(cars_dir, car_name)
        json_path = os.path.join(car_dir, "car.json")

        # Skip if already has valid car.json
        if os.path.exists(json_path):
            try:
                with open(json_path) as f:
                    cfg = json.load(f)
                if "onload" in cfg and len(cfg["onload"]) >= 2:
                    skipped_existing += 1
                    continue
            except (json.JSONDecodeError, KeyError):
                pass  # Regenerate if invalid

        # Generate config
        config = generate_car_json(car_dir, car_name)
        if config is None:
            skipped_no_data += 1
            continue

        # Write
        with open(json_path, "w") as f:
            json.dump(config, f, indent=4)

        n_on = len(config["onload"])
        n_off = len(config["offload"])
        print(f"  ✓ {config['name']:<45s} on={n_on} off={n_off} cyl={config['cylinders']}")
        created += 1

    print(f"\nDone!")
    print(f"  Created:  {created}")
    print(f"  Existing: {skipped_existing} (kept)")
    print(f"  Skipped:  {skipped_no_data} (no usable ext samples)")


if __name__ == "__main__":
    main()
