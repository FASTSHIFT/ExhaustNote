#!/usr/bin/env python3
"""
Audio quality analysis tool for ExhaustNote.
Generates objective metrics from a WAV file to evaluate crossfade quality.

Metrics:
1. Spectral Continuity Score - detects clicks/pops from layer transitions
2. Fundamental Frequency Tracking - verifies smooth RPM-to-pitch mapping
3. Spectral Flux - measures how smoothly the spectrum evolves over time

Usage:
    python3 tools/audio_quality_check.py <wav_file> [--plot]
"""

import sys
import wave
import numpy as np
from pathlib import Path


def load_wav(path):
    """Load WAV file as float32 mono."""
    with wave.open(str(path), "rb") as w:
        channels = w.getnchannels()
        sampwidth = w.getsampwidth()
        rate = w.getframerate()
        frames = w.getnframes()
        raw = w.readframes(frames)

    if sampwidth == 2:
        data = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0
    else:
        raise ValueError(f"Unsupported sample width: {sampwidth}")

    if channels == 2:
        data = data.reshape(-1, 2).mean(axis=1)

    return data, rate


def spectral_flux(data, rate, hop=512):
    """
    Compute spectral flux: frame-to-frame spectral difference.
    High spikes indicate clicks/discontinuities.
    Returns (times, flux_values).
    """
    n_fft = 2048
    num_frames = (len(data) - n_fft) // hop
    window = np.hanning(n_fft)

    prev_mag = None
    flux = []
    times = []

    for i in range(num_frames):
        start = i * hop
        frame = data[start : start + n_fft] * window
        mag = np.abs(np.fft.rfft(frame))

        if prev_mag is not None:
            # Half-wave rectified spectral flux (only increases)
            diff = mag - prev_mag
            diff[diff < 0] = 0
            flux.append(np.sum(diff))
        else:
            flux.append(0.0)

        times.append(start / rate)
        prev_mag = mag

    return np.array(times), np.array(flux)


def detect_clicks(flux, threshold_factor=5.0):
    """
    Detect clicks as spectral flux values exceeding threshold.
    Returns indices of detected clicks.
    """
    median = np.median(flux)
    std = np.std(flux)
    threshold = median + threshold_factor * std
    clicks = np.where(flux > threshold)[0]
    return clicks, threshold


def fundamental_tracking(data, rate, hop=2048, fmin=30, fmax=800):
    """
    Track fundamental frequency over time using autocorrelation.
    Returns (times, frequencies).
    """
    n_fft = 4096
    num_frames = (len(data) - n_fft) // hop
    times = []
    freqs = []

    lag_min = int(rate / fmax)
    lag_max = int(rate / fmin)

    for i in range(num_frames):
        start = i * hop
        frame = data[start : start + n_fft]
        frame = frame * np.hanning(n_fft)

        # Autocorrelation
        corr = np.correlate(frame, frame, mode="full")
        corr = corr[n_fft:]  # Only positive lags

        # Find peak in valid range
        search = corr[lag_min:lag_max]
        if len(search) > 0 and np.max(search) > 0:
            peak_idx = np.argmax(search) + lag_min
            freq = rate / peak_idx
        else:
            freq = 0.0

        times.append(start / rate)
        freqs.append(freq)

    return np.array(times), np.array(freqs)


def smoothness_score(values):
    """
    Compute smoothness: 1 - normalized jerk (third derivative).
    1.0 = perfectly smooth, 0.0 = very jerky.
    """
    if len(values) < 4:
        return 1.0
    diff1 = np.diff(values)
    diff2 = np.diff(diff1)
    diff3 = np.diff(diff2)
    jerk = np.mean(np.abs(diff3))
    signal_range = np.ptp(values)
    if signal_range < 1e-6:
        return 1.0
    normalized_jerk = jerk / signal_range
    score = max(0.0, 1.0 - normalized_jerk * 10.0)
    return score


def analyze(wav_path, do_plot=False):
    """Run full analysis on a WAV file."""
    print(f"Analyzing: {wav_path}")
    print("=" * 60)

    data, rate = load_wav(wav_path)
    duration = len(data) / rate
    print(f"  Duration: {duration:.2f}s  Rate: {rate}Hz  Samples: {len(data)}")

    # 1. Spectral Flux (click detection)
    print("\n[1] Spectral Continuity (click detection)")
    times, flux = spectral_flux(data, rate)
    clicks, threshold = detect_clicks(flux)
    flux_mean = np.mean(flux)
    flux_max = np.max(flux)
    flux_ratio = flux_max / flux_mean if flux_mean > 0 else 0

    print(f"  Mean flux: {flux_mean:.2f}")
    print(f"  Max flux:  {flux_max:.2f}")
    print(f"  Max/Mean ratio: {flux_ratio:.1f}x")
    print(f"  Detected clicks: {len(clicks)}")
    if len(clicks) > 0:
        print(f"  Click times: {times[clicks[:10]]}")

    continuity_score = max(0.0, 1.0 - len(clicks) / (duration * 10))
    print(f"  Continuity Score: {continuity_score:.2f} / 1.00")

    # 2. Fundamental Frequency Tracking
    print("\n[2] Fundamental Frequency Tracking")
    f_times, f_freqs = fundamental_tracking(data, rate)
    valid = f_freqs > 20
    if np.sum(valid) > 10:
        freq_smoothness = smoothness_score(f_freqs[valid])
        freq_range = (np.min(f_freqs[valid]), np.max(f_freqs[valid]))
        print(f"  Frequency range: {freq_range[0]:.0f} - {freq_range[1]:.0f} Hz")
        print(f"  Smoothness Score: {freq_smoothness:.2f} / 1.00")
    else:
        freq_smoothness = 0.0
        print(f"  Could not track fundamental (too noisy)")

    # 3. Overall RMS envelope smoothness
    print("\n[3] Amplitude Envelope Smoothness")
    hop = 512
    rms_frames = len(data) // hop
    rms = np.zeros(rms_frames)
    for i in range(rms_frames):
        chunk = data[i * hop : (i + 1) * hop]
        rms[i] = np.sqrt(np.mean(chunk**2))

    rms_smoothness = smoothness_score(rms)
    print(f"  RMS Smoothness: {rms_smoothness:.2f} / 1.00")

    # Overall score
    print("\n" + "=" * 60)
    overall = (continuity_score * 0.4 + freq_smoothness * 0.3 + rms_smoothness * 0.3)
    print(f"  OVERALL QUALITY SCORE: {overall:.2f} / 1.00")
    print()

    grade = "EXCELLENT" if overall > 0.85 else "GOOD" if overall > 0.7 else "FAIR" if overall > 0.5 else "POOR"
    print(f"  Grade: {grade}")
    print()

    # Machine-readable summary
    result = {
        "duration": duration,
        "clicks": len(clicks),
        "continuity_score": continuity_score,
        "freq_smoothness": freq_smoothness,
        "rms_smoothness": rms_smoothness,
        "overall_score": overall,
        "grade": grade,
    }

    if do_plot:
        try:
            import matplotlib.pyplot as plt

            fig, axes = plt.subplots(4, 1, figsize=(12, 10))

            axes[0].plot(np.arange(len(data)) / rate, data, linewidth=0.3)
            axes[0].set_title("Waveform")
            axes[0].set_xlabel("Time (s)")

            axes[1].plot(times, flux)
            axes[1].axhline(threshold, color="r", linestyle="--", label="Click threshold")
            if len(clicks) > 0:
                axes[1].scatter(times[clicks], flux[clicks], color="r", s=20, label="Clicks")
            axes[1].set_title(f"Spectral Flux (clicks={len(clicks)})")
            axes[1].legend()

            axes[2].plot(f_times, f_freqs)
            axes[2].set_title(f"Fundamental Frequency (smoothness={freq_smoothness:.2f})")
            axes[2].set_ylabel("Hz")

            axes[3].plot(np.arange(rms_frames) * hop / rate, rms)
            axes[3].set_title(f"RMS Envelope (smoothness={rms_smoothness:.2f})")
            axes[3].set_xlabel("Time (s)")

            plt.tight_layout()
            out_path = str(wav_path).replace(".wav", "_analysis.png")
            plt.savefig(out_path, dpi=100)
            print(f"  Plot saved: {out_path}")
            plt.close()
        except ImportError:
            print("  (matplotlib not available, skipping plot)")

    return result


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <wav_file> [--plot]")
        sys.exit(1)

    wav_path = sys.argv[1]
    do_plot = "--plot" in sys.argv

    if not Path(wav_path).exists():
        print(f"File not found: {wav_path}")
        sys.exit(1)

    analyze(wav_path, do_plot)
