#!/usr/bin/env python3
"""Extract WAV samples from FMOD .bank files.

Generic tool for extracting audio samples embedded in FMOD SoundBank files.
Searches for FSB5 containers within .bank files and rebuilds each sample
as a standard WAV file.

Usage:
    python3 tools/extract_fmod_bank.py <input_dir> [output_dir]

    input_dir   Directory containing .bank files (searched recursively)
    output_dir  Where to write extracted WAVs (default: ./extracted/)

Features:
    - Recursively finds all .bank files in input directory
    - Extracts embedded FSB5 audio containers
    - Converts stereo to mono (left+right average)
    - Organizes output by source bank name
    - Prints sample metadata (frequency, channels, duration)

Dependencies:
    pip install fsb5
"""

import os
import struct
import sys
from pathlib import Path

try:
    import fsb5
except ImportError:
    print("ERROR: Missing dependency. Install with: pip install fsb5")
    sys.exit(1)


def extract_fsb5_from_bank(bank_path: str) -> list[tuple[str, bytes, dict]]:
    """Extract all samples from an FMOD .bank file.

    Returns list of (name, wav_bytes, info_dict) tuples.
    info_dict contains: frequency, channels, samples (frame count).
    """
    with open(bank_path, "rb") as f:
        data = f.read()

    fsb_offset = data.find(b"FSB5")
    if fsb_offset < 0:
        return []

    try:
        fsb = fsb5.FSB5(data[fsb_offset:])
    except Exception as e:
        print(f"    WARN: Failed to parse FSB5 in {bank_path}: {e}")
        return []

    results = []
    for sample in fsb.samples:
        try:
            wav_data = fsb.rebuild_sample(sample)
            info = {
                "frequency": sample.frequency,
                "channels": sample.channels,
                "frames": sample.samples,
            }
            results.append((sample.name, wav_data, info))
        except Exception:
            pass  # Skip broken samples

    return results


def stereo_to_mono(wav_data: bytes) -> bytes:
    """Convert stereo 16-bit WAV to mono by averaging L+R channels."""
    if len(wav_data) < 44:
        return wav_data

    channels = struct.unpack_from("<H", wav_data, 22)[0]
    if channels != 2:
        return wav_data

    sample_rate = struct.unpack_from("<I", wav_data, 24)[0]
    bits_per_sample = struct.unpack_from("<H", wav_data, 34)[0]
    if bits_per_sample != 16:
        return wav_data

    # Find data chunk
    data_offset = wav_data.find(b"data")
    if data_offset < 0:
        return wav_data
    data_size = struct.unpack_from("<I", wav_data, data_offset + 4)[0]
    pcm_start = data_offset + 8

    # Average L+R
    n_frames = data_size // 4
    mono_samples = []
    for i in range(n_frames):
        offset = pcm_start + i * 4
        if offset + 4 > len(wav_data):
            break
        left = struct.unpack_from("<h", wav_data, offset)[0]
        right = struct.unpack_from("<h", wav_data, offset + 2)[0]
        mono_samples.append((left + right) // 2)

    # Build mono WAV
    mono_data_size = len(mono_samples) * 2
    header = struct.pack(
        "<4sI4s4sIHHIIHH4sI",
        b"RIFF",
        36 + mono_data_size,
        b"WAVE",
        b"fmt ",
        16,
        1,  # PCM
        1,  # mono
        sample_rate,
        sample_rate * 2,
        2,
        16,
        b"data",
        mono_data_size,
    )
    pcm = b"".join(struct.pack("<h", s) for s in mono_samples)
    return header + pcm


def process_bank(bank_path: str, output_dir: str, to_mono: bool = True) -> int:
    """Process one .bank file, extract all samples.

    Returns number of samples extracted.
    """
    bank_name = Path(bank_path).stem
    samples = extract_fsb5_from_bank(bank_path)

    if not samples:
        return 0

    out_dir = os.path.join(output_dir, bank_name)
    os.makedirs(out_dir, exist_ok=True)

    count = 0
    for name, wav_data, info in samples:
        # Convert stereo to mono if requested
        if to_mono and info["channels"] == 2:
            wav_data = stereo_to_mono(wav_data)

        # Sanitize filename
        safe_name = "".join(c if c.isalnum() or c in "_-" else "_" for c in name)
        safe_name = safe_name.strip("_").lower()
        if not safe_name:
            safe_name = f"sample_{count}"

        out_path = os.path.join(out_dir, safe_name + ".wav")
        with open(out_path, "wb") as f:
            f.write(wav_data)
        count += 1

        duration = info["frames"] / info["frequency"] if info["frequency"] > 0 else 0
        ch_str = "mono" if info["channels"] == 1 or to_mono else f"{info['channels']}ch"
        print(f"    {safe_name}.wav  ({info['frequency']}Hz {ch_str} {duration:.1f}s)")

    return count


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 tools/extract_fmod_bank.py <input_dir> [output_dir]")
        print()
        print("  input_dir   Directory containing .bank files (searched recursively)")
        print("  output_dir  Where to write extracted WAVs (default: ./extracted/)")
        print()
        print("Options (via environment variables):")
        print("  MONO=0      Keep stereo (default: convert to mono)")
        sys.exit(1)

    input_dir = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "extracted"
    to_mono = os.environ.get("MONO", "1") != "0"

    if not os.path.isdir(input_dir):
        print(f"ERROR: {input_dir} is not a directory")
        sys.exit(1)

    print(f"Scanning: {input_dir}")
    print(f"Output:   {output_dir}/")
    print(f"Mono:     {'yes' if to_mono else 'no'}")
    print()

    # Find all .bank files recursively
    bank_files = sorted(Path(input_dir).rglob("*.bank"))
    if not bank_files:
        print("No .bank files found.")
        sys.exit(1)

    print(f"Found {len(bank_files)} bank file(s)\n")

    total_samples = 0
    total_banks = 0

    for bank_path in bank_files:
        rel_path = bank_path.relative_to(input_dir)
        print(f"  [{rel_path}]")
        count = process_bank(str(bank_path), output_dir, to_mono)
        if count > 0:
            total_banks += 1
            total_samples += count

    print(f"\nDone! Extracted {total_samples} samples from {total_banks} banks.")
    print(f"Output: {output_dir}/")


if __name__ == "__main__":
    main()
