#!/usr/bin/env python3
"""Generate synthetic demo car sound packs for ExhaustNote.

Creates WAV files using additive synthesis to simulate engine sounds at
various RPMs. No copyrighted material — pure math-generated audio.

Usage:
    python3 tools/generate_demo_cars.py [output_dir]
    Default output: ./cars/
"""

import json
import math
import os
import random
import struct
import sys

SAMPLE_RATE = 44100


def write_wav(path: str, samples: list[float], sample_rate: int = SAMPLE_RATE):
    """Write mono 16-bit WAV file."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    n = len(samples)
    data_size = n * 2
    with open(path, "wb") as f:
        # RIFF header
        f.write(b"RIFF")
        f.write(struct.pack("<I", 36 + data_size))
        f.write(b"WAVE")
        # fmt chunk
        f.write(b"fmt ")
        f.write(struct.pack("<IHHIIHH", 16, 1, 1, sample_rate, sample_rate * 2, 2, 16))
        # data chunk
        f.write(b"data")
        f.write(struct.pack("<I", data_size))
        for s in samples:
            val = max(-1.0, min(1.0, s))
            f.write(struct.pack("<h", int(val * 32767)))


def get_firing_angles(cylinders: int, engine_type: str = "") -> list[float]:
    """Return firing angles in degrees for one complete 720° cycle (4-stroke).

    Different engine configurations have different firing intervals,
    which is the primary source of their unique sound character.
    """
    if cylinders == 4:
        # I4: equal 180° intervals — smooth, buzzy
        return [0, 180, 360, 540]
    elif cylinders == 6:
        # V6 60°: equal 120° intervals — smooth
        return [0, 120, 240, 360, 480, 600]
    elif cylinders == 8:
        if "flat" in engine_type.lower():
            # Flat-plane V8 (Ferrari 458): equal 90° — screaming
            return [0, 90, 180, 270, 360, 450, 540, 630]
        else:
            # Cross-plane V8 (American muscle): uneven intervals
            # Firing order 1-8-4-3-6-5-7-2, gives the "burble"
            return [0, 90, 180, 270, 450, 540, 630, 720 - 90]
    elif cylinders == 10:
        # V10 72°: equal intervals — distinctive wail
        return [i * 72 for i in range(10)]
    elif cylinders == 12:
        # V12 60°: equal intervals — silky smooth
        return [i * 60 for i in range(12)]
    else:
        # Generic equal spacing
        interval = 720.0 / cylinders
        return [i * interval for i in range(cylinders)]


def generate_exhaust_pulse(pulse_width: float, onload: bool) -> list[float]:
    """Generate a single exhaust valve opening pulse.

    Models the pressure wave from one cylinder's exhaust event.
    Uses a smooth raised-cosine envelope to avoid harsh transients.
    """
    n = int(pulse_width * SAMPLE_RATE)
    pulse = [0.0] * n

    for i in range(n):
        t = i / SAMPLE_RATE
        # Smooth envelope: raised cosine attack + exponential decay
        # This avoids the sharp edges that cause "tearing" artifacts
        attack_time = pulse_width * 0.15
        if t < attack_time:
            env = 0.5 * (1.0 - math.cos(math.pi * t / attack_time))
        else:
            env = math.exp(-(t - attack_time) * (200 if onload else 300))

        # Primary pipe resonance (low, round tone)
        freq = 90.0 if onload else 70.0
        pulse[i] = env * math.sin(2 * math.pi * freq * t)
        # Sub-bass body (the "thump" feel)
        pulse[i] += env * 0.7 * math.sin(2 * math.pi * 45 * t)
        # Mild second harmonic (warmth, not brightness)
        pulse[i] += env * 0.2 * math.sin(2 * math.pi * freq * 2.0 * t)

    return pulse


def generate_engine_tone(
    rpm: float,
    cylinders: int,
    duration: float = 2.0,
    onload: bool = True,
    engine_type: str = "",
) -> list[float]:
    """Generate engine exhaust sound based on firing order pulse train.

    Core principle: each cylinder fires at specific crank angles.
    The exhaust sound is the superposition of individual exhaust pulses
    arriving at the tailpipe according to the firing order timing.

    Different cylinder counts and configurations (cross-plane vs flat-plane)
    produce fundamentally different rhythmic patterns.
    """
    n_samples = int(duration * SAMPLE_RATE)
    samples = [0.0] * n_samples

    # Get firing angles for this engine type
    firing_angles = get_firing_angles(cylinders, engine_type)

    # Time for one complete engine cycle (720° of crank rotation)
    cycle_time = 60.0 / rpm * 2.0  # 2 revolutions per cycle (4-stroke)

    # Generate the exhaust pulse shape
    # Pulse width depends on RPM (higher RPM = shorter pulses)
    pulse_width = min(0.02, cycle_time / cylinders * 0.8)
    pulse = generate_exhaust_pulse(pulse_width, onload)
    pulse_len = len(pulse)

    # Place pulses according to firing order timing
    t = 0.0
    while t < duration:
        for angle in firing_angles:
            # Convert crank angle to time offset within this cycle
            pulse_time = t + (angle / 720.0) * cycle_time
            pulse_sample_idx = int(pulse_time * SAMPLE_RATE)

            if pulse_sample_idx >= n_samples:
                break

            # Add pulse with slight random variation (mechanical imperfection)
            amp_var = 1.0 + random.uniform(-0.05, 0.05)
            for j in range(min(pulse_len, n_samples - pulse_sample_idx)):
                samples[pulse_sample_idx + j] += pulse[j] * amp_var

        t += cycle_time

    # === Post-processing: exhaust pipe + muffler filtering ===
    # Three-pass cascaded low-pass to get smooth, deep tone
    # Lower cutoff = more muffled, removes all "tearing" artifacts
    cutoff = 250.0 if onload else 150.0
    rc = 1.0 / (2.0 * math.pi * cutoff)
    lp_alpha = 1.0 / (1.0 + SAMPLE_RATE * rc)

    # Pass 1
    prev = 0.0
    for i in range(n_samples):
        prev += lp_alpha * (samples[i] - prev)
        samples[i] = prev

    # Pass 2
    prev = 0.0
    for i in range(n_samples):
        prev += lp_alpha * (samples[i] - prev)
        samples[i] = prev

    # Pass 3 (extra smoothing — kills remaining harshness)
    prev = 0.0
    for i in range(n_samples):
        prev += lp_alpha * (samples[i] - prev)
        samples[i] = prev

    # Add subtle pipe resonance feedback (standing wave in exhaust)
    pipe_delay = int(SAMPLE_RATE / 90.0)  # longer pipe → lower resonance
    feedback = 0.2 if onload else 0.15
    for i in range(pipe_delay, n_samples):
        samples[i] += samples[i - pipe_delay] * feedback

    # === DC removal via high-pass filter (20Hz cutoff) ===
    # Simple first-order HP: y[n] = alpha * (y[n-1] + x[n] - x[n-1])
    # This removes DC and very-low-frequency drift that causes speaker distortion.
    hp_cutoff = 20.0
    hp_rc = 1.0 / (2.0 * math.pi * hp_cutoff)
    hp_alpha = hp_rc / (hp_rc + 1.0 / SAMPLE_RATE)
    prev_in = 0.0
    prev_out = 0.0
    for i in range(n_samples):
        prev_out = hp_alpha * (prev_out + samples[i] - prev_in)
        prev_in = samples[i]
        samples[i] = prev_out

    # === Add filtered white noise (exhaust turbulence / air rush) ===
    # This gives the sound a natural "dirty" texture instead of pure tones.
    # Noise is low-pass filtered to avoid harsh hiss.
    noise_level = 0.06 if onload else 0.04
    noise_cutoff = 200.0  # only low-frequency rumble noise
    noise_rc = 1.0 / (2.0 * math.pi * noise_cutoff)
    noise_alpha = 1.0 / (1.0 + SAMPLE_RATE * noise_rc)
    noise_prev = 0.0
    for i in range(n_samples):
        raw_noise = random.uniform(-1, 1)
        noise_prev += noise_alpha * (raw_noise - noise_prev)
        samples[i] += noise_prev * noise_level

    # Normalize (now centered around zero)
    peak = max(abs(s) for s in samples) or 1.0
    samples = [s / peak * 0.85 for s in samples]

    # Fade in/out (50ms)
    fade_samples = int(0.05 * SAMPLE_RATE)
    for i in range(min(fade_samples, n_samples)):
        samples[i] *= i / fade_samples
        samples[-(i + 1)] *= i / fade_samples

    return samples


def generate_backfire(duration: float = 1.0) -> list[float]:
    """Generate a synthetic exhaust 'bang' — short deep thump like a DCT shift.

    Character: fast attack, low-frequency pressure pulse, quick decay.
    Should sound like a muffled cannon shot through an exhaust pipe.
    """
    n_samples = int(duration * SAMPLE_RATE)
    samples = [0.0] * n_samples

    # === Phase 1: Initial pressure slam (0-5ms) ===
    # Sharp attack — air pressure burst hitting the pipe
    slam_len = int(0.005 * SAMPLE_RATE)
    for i in range(slam_len):
        t = i / slam_len
        # Half-sine pulse (like a speaker cone punch)
        samples[i] = math.sin(math.pi * t) * 1.0

    # === Phase 2: Pipe resonance (main body of the "bang") ===
    # Damped oscillation at exhaust pipe natural frequency (~60-80Hz)
    # This is what gives it the deep "BONG" character
    for i in range(n_samples):
        t = i / SAMPLE_RATE
        # Fast exponential decay (bang, not sustained drone)
        decay = math.exp(-t * 12)
        # Primary pipe resonance
        samples[i] += decay * 0.9 * math.sin(2 * math.pi * 65 * t)
        # Second harmonic (gives body)
        samples[i] += decay * 0.5 * math.sin(2 * math.pi * 130 * t + 0.3)
        # Sub-bass thump (chest punch feel)
        samples[i] += decay * 0.7 * math.sin(2 * math.pi * 35 * t)

    # === Phase 3: Brief tail rumble (exhaust turbulence) ===
    random.seed(42)
    tail_start = int(0.05 * SAMPLE_RATE)
    tail_end = int(0.3 * SAMPLE_RATE)
    for i in range(tail_start, min(tail_end, n_samples)):
        t = (i - tail_start) / SAMPLE_RATE
        decay = math.exp(-t * 15)
        # Low-frequency filtered noise (rumble, not crackle)
        samples[i] += decay * 0.2 * random.uniform(-1, 1)

    # === Low-pass filter (muffler + distance) ===
    # Very low cutoff — only the deep thump comes through
    cutoff = 150.0
    rc = 1.0 / (2.0 * math.pi * cutoff)
    lp_alpha = 1.0 / (1.0 + SAMPLE_RATE * rc)
    prev = 0.0
    for i in range(n_samples):
        prev += lp_alpha * (samples[i] - prev)
        samples[i] = prev

    # Normalize
    peak = max(abs(s) for s in samples) or 1.0
    samples = [s / peak * 0.9 for s in samples]

    # Hard fade-out at end (prevent click)
    fade_out = int(0.02 * SAMPLE_RATE)
    for i in range(min(fade_out, n_samples)):
        samples[-(i + 1)] *= i / fade_out

    return samples


# --- Car definitions (public specs, no copyrighted data) ---
CARS = [
    {
        "dir": "demo_v8_muscle",
        "engine_type": "crossplane",  # Uneven firing → classic burble
        "config": {
            "name": "Demo V8 Muscle (Cross-plane)",
            "engine": "V8 5.0L NA",
            "cylinders": 8,
            "rpm_idle": 750,
            "rpm_redline": 7000,
            "peak_torque": 460,
            "peak_torque_rpm": 4500,
            "inertia": 0.15,
            "transmission": {
                "gears": [2.66, 1.78, 1.30, 1.00, 0.74, 0.50],
                "final_drive": 3.73,
            },
        },
        "rpm_layers": [750, 2500, 4000, 5500, 6800],
    },
    {
        "dir": "demo_v8_flatplane",
        "engine_type": "flatplane",  # Even firing → screaming
        "config": {
            "name": "Demo V8 Flat-plane",
            "engine": "V8 4.5L NA (Flat-plane)",
            "cylinders": 8,
            "rpm_idle": 900,
            "rpm_redline": 9000,
            "peak_torque": 530,
            "peak_torque_rpm": 6000,
            "inertia": 0.10,
            "transmission": {
                "gears": [3.08, 2.19, 1.63, 1.29, 1.03, 0.84, 0.69],
                "final_drive": 4.44,
            },
        },
        "rpm_layers": [900, 3000, 5000, 7000, 8800],
    },
    {
        "dir": "demo_v6_turbo",
        "config": {
            "name": "Demo V6 Twin-Turbo",
            "engine": "V6 3.0L TT",
            "cylinders": 6,
            "rpm_idle": 800,
            "rpm_redline": 7500,
            "peak_torque": 500,
            "peak_torque_rpm": 3500,
            "inertia": 0.10,
            "transmission": {
                "gears": [3.97, 2.34, 1.52, 1.14, 0.87, 0.69, 0.56],
                "final_drive": 3.46,
            },
        },
        "rpm_layers": [800, 2500, 4000, 5500, 7200],
    },
    {
        "dir": "demo_i4_sport",
        "config": {
            "name": "Demo I4 Sport",
            "engine": "I4 2.0L Turbo",
            "cylinders": 4,
            "rpm_idle": 850,
            "rpm_redline": 7200,
            "peak_torque": 350,
            "peak_torque_rpm": 4000,
            "inertia": 0.08,
            "transmission": {
                "gears": [3.54, 2.13, 1.52, 1.18, 0.97, 0.82],
                "final_drive": 4.06,
            },
        },
        "rpm_layers": [850, 2500, 4000, 5500, 7000],
    },
    {
        "dir": "demo_v10_supercar",
        "config": {
            "name": "Demo V10 Supercar",
            "engine": "V10 5.2L NA",
            "cylinders": 10,
            "rpm_idle": 900,
            "rpm_redline": 8500,
            "peak_torque": 560,
            "peak_torque_rpm": 6500,
            "inertia": 0.11,
            "transmission": {
                "gears": [3.91, 2.44, 1.81, 1.40, 1.12, 0.92, 0.76],
                "final_drive": 3.54,
            },
        },
        "rpm_layers": [900, 3000, 5000, 7000, 8300],
    },
    {
        "dir": "demo_v12_gt",
        "config": {
            "name": "Demo V12 Grand Tourer",
            "engine": "V12 6.5L NA",
            "cylinders": 12,
            "rpm_idle": 800,
            "rpm_redline": 8500,
            "peak_torque": 690,
            "peak_torque_rpm": 5500,
            "inertia": 0.14,
            "transmission": {
                "gears": [3.15, 2.18, 1.57, 1.19, 0.94, 0.76, 0.63],
                "final_drive": 3.77,
            },
        },
        "rpm_layers": [800, 2500, 4500, 6500, 8200],
    },
]


def generate_car(car: dict, output_dir: str):
    """Generate all WAV files and car.json for one car."""
    car_dir = os.path.join(output_dir, car["dir"])
    os.makedirs(car_dir, exist_ok=True)

    config = car["config"]
    cylinders = config["cylinders"]
    rpm_layers = car["rpm_layers"]

    onload_entries = []
    offload_entries = []

    engine_type = car.get("engine_type", "")

    for rpm in rpm_layers:
        # Onload
        on_name = f"on_{int(rpm)}.wav"
        on_path = os.path.join(car_dir, on_name)
        samples = generate_engine_tone(
            rpm, cylinders, duration=2.0, onload=True, engine_type=engine_type
        )
        write_wav(on_path, samples)
        onload_entries.append({"file": on_name, "rpm": rpm})

        # Offload
        off_name = f"off_{int(rpm)}.wav"
        off_path = os.path.join(car_dir, off_name)
        samples = generate_engine_tone(
            rpm, cylinders, duration=2.0, onload=False, engine_type=engine_type
        )
        write_wav(off_path, samples)
        offload_entries.append({"file": off_name, "rpm": rpm})

    config["onload"] = onload_entries
    config["offload"] = offload_entries

    # Write car.json
    json_path = os.path.join(car_dir, "car.json")
    with open(json_path, "w") as f:
        json.dump(config, f, indent=4)

    print(f"  ✓ {config['name']} ({len(rpm_layers)} layers)")


def main():
    output_dir = sys.argv[1] if len(sys.argv) > 1 else "cars"

    # Remove old demo cars (keep non-demo ones)
    print(f"Generating demo cars in {output_dir}/")

    # Generate backfire sample
    backfire_dir = os.path.join(output_dir, "backfire")
    os.makedirs(backfire_dir, exist_ok=True)
    backfire = generate_backfire()
    write_wav(os.path.join(backfire_dir, "backfireEXT_1.wav"), backfire)
    print("  ✓ Backfire sample")

    # Generate each car
    for car in CARS:
        generate_car(car, output_dir)

    print(f"\nDone! {len(CARS)} cars generated.")
    print(f"Total WAV files: {len(CARS) * 10 + 1}")
    print(f"\nRun the simulator: ./build/sim/app/sim_gui/exhaust_sim_gui")


if __name__ == "__main__":
    main()
