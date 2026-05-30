#!/usr/bin/env python3
"""
Quick crossfade test using extracted Assetto Corsa Ferrari 458 samples.
Generates a WAV file with RPM sweep from idle to redline using multi-layer crossfade.

LEGAL NOTICE:
This tool is provided for personal/educational use only.
Extracting audio from commercial games may violate their EULA.
Users are solely responsible for ensuring compliance with
applicable licenses and copyright laws.
This project does NOT distribute any copyrighted audio data.
"""

import numpy as np
import wave
import os
import struct

SAMPLE_RATE = 44100
OUTPUT_FILE = "/tmp/ac_ferrari_458/crossfade_sweep.wav"

# Layer definitions: (filename, RPM)
# Using external onload samples from Ferrari 458
LAYERS = [
    ("/tmp/ac_ferrari_458/F4CH_IDLE_EXT.wav", 900),
    ("/tmp/ac_ferrari_458/ext_on3500.wav", 3500),
    ("/tmp/ac_ferrari_458/ext_on5000.wav", 5000),
    ("/tmp/ac_ferrari_458/ext_on6750.wav", 6750),
    ("/tmp/ac_ferrari_458/ext_on8500.wav", 8500),
]


def load_wav_mono(path):
    """Load a WAV file and convert to mono float32 [-1, 1]."""
    with wave.open(path, "rb") as w:
        channels = w.getnchannels()
        sampwidth = w.getsampwidth()
        frames = w.getnframes()
        raw = w.readframes(frames)

    if sampwidth == 2:
        data = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0
    elif sampwidth == 1:
        data = (np.frombuffer(raw, dtype=np.uint8).astype(np.float32) - 128.0) / 128.0
    else:
        raise ValueError(f"Unsupported sample width: {sampwidth}")

    # Convert to mono if stereo
    if channels == 2:
        data = data.reshape(-1, 2).mean(axis=1)

    return data


def crossfade_process(layers_data, layer_rpms, rpm, phase_positions, chunk_size):
    """
    Process one chunk with multi-layer crossfade.
    Returns (output_chunk, updated_phase_positions).
    """
    num_layers = len(layers_data)

    # Find adjacent layers
    if rpm <= layer_rpms[0]:
        lo_idx, hi_idx, mix = 0, 0, 0.0
    elif rpm >= layer_rpms[-1]:
        lo_idx, hi_idx, mix = num_layers - 1, num_layers - 1, 0.0
    else:
        for i in range(num_layers - 1):
            if layer_rpms[i] <= rpm < layer_rpms[i + 1]:
                lo_idx = i
                hi_idx = i + 1
                rng = layer_rpms[i + 1] - layer_rpms[i]
                mix = (rpm - layer_rpms[i]) / rng if rng > 0 else 0.0
                break

    # Read from both layers with looping
    output = np.zeros(chunk_size, dtype=np.float32)

    for layer_idx, weight in [(lo_idx, 1.0 - mix), (hi_idx, mix)]:
        if weight <= 0.0:
            continue
        data = layers_data[layer_idx]
        pos = phase_positions[layer_idx]
        length = len(data)

        # Extract chunk with looping
        chunk = np.zeros(chunk_size, dtype=np.float32)
        for i in range(chunk_size):
            chunk[i] = data[int(pos) % length]
            pos += 1.0

        phase_positions[layer_idx] = pos % length
        output += chunk * weight

    return output, phase_positions


def main():
    print("Loading samples...")
    layers_data = []
    layer_rpms = []

    for path, rpm in LAYERS:
        if not os.path.exists(path):
            print(f"  SKIP (not found): {path}")
            continue
        data = load_wav_mono(path)
        layers_data.append(data)
        layer_rpms.append(rpm)
        print(f"  Loaded: {os.path.basename(path):25s} RPM={rpm:5d} samples={len(data):8d} ({len(data)/SAMPLE_RATE:.2f}s)")

    if len(layers_data) < 2:
        print("ERROR: Need at least 2 layers")
        return

    # Generate RPM sweep: idle -> redline -> idle (10 seconds total)
    duration = 10.0
    total_samples = int(duration * SAMPLE_RATE)
    chunk_size = 512

    # RPM profile: ramp up for 6s, hold 1s, ramp down 3s
    rpm_profile = np.zeros(total_samples)
    rpm_min, rpm_max = layer_rpms[0], layer_rpms[-1]

    t = np.arange(total_samples) / SAMPLE_RATE
    # Phase 1: 0-5s ramp up
    mask1 = t < 5.0
    rpm_profile[mask1] = rpm_min + (rpm_max - rpm_min) * (t[mask1] / 5.0)
    # Phase 2: 5-6s hold at redline
    mask2 = (t >= 5.0) & (t < 6.0)
    rpm_profile[mask2] = rpm_max
    # Phase 3: 6-10s ramp down
    mask3 = t >= 6.0
    rpm_profile[mask3] = rpm_max - (rpm_max - rpm_min) * ((t[mask3] - 6.0) / 4.0)

    print(f"\nGenerating {duration}s RPM sweep ({rpm_min:.0f} -> {rpm_max:.0f} -> {rpm_min:.0f})...")

    # Process in chunks
    output = np.zeros(total_samples, dtype=np.float32)
    phase_positions = [0.0] * len(layers_data)

    num_chunks = total_samples // chunk_size
    for c in range(num_chunks):
        start = c * chunk_size
        rpm = rpm_profile[start + chunk_size // 2]  # Use mid-chunk RPM

        chunk, phase_positions = crossfade_process(
            layers_data, layer_rpms, rpm, phase_positions, chunk_size
        )
        output[start : start + chunk_size] = chunk

        if c % 200 == 0:
            print(f"  {start/SAMPLE_RATE:.1f}s RPM={rpm:.0f}")

    # Normalize
    peak = np.max(np.abs(output))
    if peak > 0:
        output = output / peak * 0.9

    # Write WAV
    output_int16 = (output * 32767).astype(np.int16)
    with wave.open(OUTPUT_FILE, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SAMPLE_RATE)
        w.writeframes(output_int16.tobytes())

    print(f"\nOutput: {OUTPUT_FILE}")
    print(f"  Duration: {duration}s")
    print(f"  Size: {os.path.getsize(OUTPUT_FILE) / 1024:.0f} KB")
    print(f"  Layers used: {len(layers_data)}")
    print(f"\nPlay with: aplay {OUTPUT_FILE}")
    print(f"  or: ffplay -nodisp {OUTPUT_FILE}")


if __name__ == "__main__":
    main()
