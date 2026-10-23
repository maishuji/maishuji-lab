#!/usr/bin/env python3
"""Generate the original mono PCM source for the streaming-audio lesson."""

import argparse
import math
from pathlib import Path
import struct
import wave

SAMPLE_RATE = 22_050
DURATION_SECONDS = 8
AMPLITUDE = 10_000
NOTES_HZ = (220.00, 277.18, 329.63, 440.00, 329.63, 277.18, 246.94, 220.00)


def sample_at(index: int) -> int:
    beat_samples = SAMPLE_RATE
    beat = min(index // beat_samples, len(NOTES_HZ) - 1)
    within = index % beat_samples
    phase = 2.0 * math.pi * NOTES_HZ[beat] * within / SAMPLE_RATE
    harmonic = 0.22 * math.sin(phase * 2.0)
    envelope_samples = SAMPLE_RATE // 40
    attack = min(1.0, within / envelope_samples)
    release = min(1.0, (beat_samples - 1 - within) / envelope_samples)
    envelope = min(attack, release)
    return round(AMPLITUDE * envelope * (math.sin(phase) + harmonic) / 1.22)


def write_wav(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        frames = bytearray()
        for index in range(SAMPLE_RATE * DURATION_SECONDS):
            frames.extend(struct.pack("<h", sample_at(index)))
        output.writeframes(frames)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    write_wav(args.output)


if __name__ == "__main__":
    main()
