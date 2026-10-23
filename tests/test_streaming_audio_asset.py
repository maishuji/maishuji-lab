"""Validate the original Ogg/Vorbis lesson asset and its PCM source generator."""

import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
import wave


ROOT = Path(__file__).resolve().parents[1]
ASSET = ROOT / "advanced_examples/assets/audio/streaming-theme.ogg"
GENERATOR = ROOT / "tools/generate-streaming-audio.py"
EXPECTED_SHA256 = "4ca2330d6c43f0b92161ae21398df19a5490e950903e19f67315d3823c5e97dc"
EXPECTED_RATE = 22_050
EXPECTED_SAMPLES = EXPECTED_RATE * 8


def ogg_pages(data: bytes):
    offset = 0
    while offset < len(data):
        if data[offset:offset + 4] != b"OggS":
            raise ValueError(f"missing Ogg page at byte {offset}")
        segment_count = data[offset + 26]
        header_end = offset + 27 + segment_count
        body_size = sum(data[offset + 27:header_end])
        page_end = header_end + body_size
        if page_end > len(data):
            raise ValueError("truncated Ogg page")
        yield data[offset:page_end]
        offset = page_end


class StreamingAudioAssetTests(unittest.TestCase):
    def test_committed_ogg_is_expected_mono_vorbis_stream(self):
        data = ASSET.read_bytes()
        self.assertLess(len(data), 20_000)
        self.assertEqual(hashlib.sha256(data).hexdigest(), EXPECTED_SHA256)

        identification = data.find(b"\x01vorbis")
        self.assertGreaterEqual(identification, 0)
        self.assertEqual(data[identification + 11], 1)
        self.assertEqual(struct.unpack_from("<I", data, identification + 12)[0],
                         EXPECTED_RATE)

        pages = list(ogg_pages(data))
        self.assertGreaterEqual(len(pages), 3)
        final_granule = struct.unpack_from("<Q", pages[-1], 6)[0]
        self.assertEqual(final_granule, EXPECTED_SAMPLES)

    def test_pcm_generator_is_deterministic_and_matches_asset_contract(self):
        with tempfile.TemporaryDirectory() as directory:
            first = Path(directory) / "first.wav"
            second = Path(directory) / "second.wav"
            for output in (first, second):
                subprocess.run(
                    ["python3", "-B", str(GENERATOR), "--output", str(output)],
                    cwd=ROOT, check=True, timeout=30,
                )
            self.assertEqual(first.read_bytes(), second.read_bytes())
            with wave.open(str(first), "rb") as source:
                self.assertEqual(source.getnchannels(), 1)
                self.assertEqual(source.getsampwidth(), 2)
                self.assertEqual(source.getframerate(), EXPECTED_RATE)
                self.assertEqual(source.getnframes(), EXPECTED_SAMPLES)


if __name__ == "__main__":
    unittest.main()
