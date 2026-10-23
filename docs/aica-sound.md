# AICA sound: example 16

[Lesson index](examples.md) · [next-lesson plan](next-lessons.md)

`examples/16-aica-sound.cpp` introduces sound independently of PVR and input.
The original 440 Hz 16-bit mono PCM tone is generated in fixed storage in
[`aica_tone.hpp`](../examples/aica_tone.hpp), with a short attack and release
to avoid a click. It contains 4096 samples at 11025 Hz (about 372 ms,
8192 bytes). The raw buffer meets pinned KOS's 32-byte alignment-of-length
requirement and 65534-sample limit; `snd_sfx_load_raw_buf()` copies it into
AICA sound RAM before returning. No external audio asset or new framework
sound owner is required.

The example calls the pinned KOS `snd_init()`, checks the returned SFX handle,
then requests the same mono tone at left, center and right pan. It checks each
`snd_sfx_play()` return value, waits 500 ms between requests (longer than the
sound), and unloads the effect after playback. It deliberately does **not**
call `snd_shutdown()`; KOS's normal shutdown owns system-wide audio teardown.
It ends after roughly 1.5 seconds. If initialization, load or playback fails,
it logs the failure and returns nonzero.

## Build and listen

```sh
make host-test-aica-tone
make in-container TARGET=dreamcast-aica-sound-cdi DC_BUILD_TYPE=Release
```

This produces `build-dreamcast/maishuji-aica-sound.cdi`. Boot it in a configured
Flycast with audio output enabled, or on a configured Dreamcast, and listen for
three short tones moving left, center and right. The guest log should end with
`maishuji: AICA sound playback requests passed (left/center/right; audibility unverified)`.
**No automated audio-output capture or physical-console listening was performed**
when this lesson was added. A log marker proves only that requests succeeded,
not that any sound reached speakers. The existing
[`test-flycast-render.sh`](../tools/test-flycast-render.sh) captures the game
window, not audio: do not use its screenshot as an audio acceptance gate.

## KOS boundary and verification

The behavior was checked against the exact pinned KOS `63702a8`
[sfxmgr.h](https://github.com/KallistiOS/KallistiOS/blob/63702a858c17c915b564b378407b1576c78668ec/kernel/arch/dreamcast/include/dc/sound/sfxmgr.h),
[sound.h](https://github.com/KallistiOS/KallistiOS/blob/63702a858c17c915b564b378407b1576c78668ec/kernel/arch/dreamcast/include/dc/sound/sound.h)
and [sample implementation](https://github.com/KallistiOS/KallistiOS/blob/63702a858c17c915b564b378407b1576c78668ec/kernel/arch/dreamcast/sound/snd_sfxmgr.c).
`make host-test-aica-tone` verifies deterministic waveform bounds and silent
ends, not hardware output. The pinned Debug/Release builds check the API and
linker, and CDI packaging checks the image path. Real audio output, channel
panning and timing remain manual gates on a named Flycast/audio or Dreamcast
configuration. Keep later streaming, mixing and application-level audio
ownership separate from this foundation lesson.
