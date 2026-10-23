# Streaming-audio lesson asset

`streaming-theme.ogg` is an original eight-second mono composition created for
maishuji-lab. It is licensed under the same terms as this repository and does
not incorporate third-party music or samples.

The reproducible source is generated at 22050 Hz, signed 16-bit mono PCM:

```sh
python3 -B tools/generate-streaming-audio.py \
  --output build-audio-asset/streaming-theme.wav
```

The committed Ogg Vorbis file was encoded inside the project's pinned toolchain
container, which currently supplies FFmpeg 5.1.8 and libvorbis:

```sh
./tools/with-kos.sh ffmpeg -hide_banner -y \
  -i build-audio-asset/streaming-theme.wav \
  -map_metadata -1 -fflags +bitexact -flags:a +bitexact \
  -c:a libvorbis -q:a 3 -ac 1 -ar 22050 -serial_offset 4242 \
  advanced_examples/assets/audio/streaming-theme.ogg
```

Expected SHA-256:

```text
4ca2330d6c43f0b92161ae21398df19a5490e950903e19f67315d3823c5e97dc
```

The asset is deliberately mono. The pinned KOS revision predates the correction
discussed in KallistiOS PR #1557 for PCM16 stereo channel splitting, so this
lesson does not claim left/right channel correctness.
