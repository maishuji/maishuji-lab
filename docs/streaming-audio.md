# Streaming audio: advanced example 13

[Lesson index](examples.md) · [AICA sound foundation](aica-sound.md) · [next-lesson plan](next-lessons.md)

`advanced_examples/13-streaming-audio.cpp` extends the short PCM sound-effect lesson with a compressed music path. The executable reads an original mono Ogg Vorbis asset from its embedded ROM disk, decodes it on the SH-4, and supplies PCM blocks to KallistiOS `snd_stream` for playback by the AICA.

The important distinction is that **Ogg is the container and Vorbis is the audio codec**. The AICA does not decode either format. `liboggvorbisplay` and `libvorbis` run on the SH-4, produce PCM, and keep the KOS stream supplied. Compression saves disc or ROM-disk space, but decoding consumes CPU time and an interrupted producer can cause an audible underrun.

## Why this format

This lesson uses an 8-second, 22,050 Hz, mono Vorbis loop at quality 3. The committed file is 13,560 bytes; its deterministic PCM source would occupy 352,800 bytes as signed 16-bit mono samples. The exact generation and encoding commands, checksum, and asset contract are in [`advanced_examples/assets/audio/README.md`](../advanced_examples/assets/audio/README.md).

Vorbis is the default recommendation for this lesson because the pinned kos-ports revision provides a small KOS playback wrapper and permissively licensed decoder dependencies. MP3 is not the default because the pinned `libmp3` port wraps the GPLv2-or-later XingMP3 implementation, which introduces a different licensing decision for an application. Opus is useful for voice and network-oriented work but adds complexity that does not help explain the first music stream. ADX is game-oriented, but it is a proprietary CRI format and should be evaluated in a separate lesson.

The first streaming lesson is deliberately mono. The pinned KOS commit `63702a858c17c915b564b378407b1576c78668ec` predates the stereo PCM16 left/right splitting work tracked in [KallistiOS PR #1557](https://github.com/KallistiOS/KallistiOS/pull/1557). Mono keeps the example aligned with the verified pinned API rather than claiming stereo behavior from newer work.

## Runtime ownership

The lesson-local `VorbisTrack` owns initialization and shutdown in this order:

1. initialize KOS `snd_stream`;
2. initialize the Ogg Vorbis playback wrapper;
3. start the ROM-disk track and apply volume;
4. stop only after a successful start;
5. shut down the decoder before the stream subsystem.

Cleanup is idempotent so early returns and normal exit release the same resources. This ownership remains local to the executable; it does not turn the rendering framework into a global audio manager.

Live controls are:

- `A`: restart the looping track;
- `B`: stop;
- `X`: play;
- left/right triggers: decrease/increase volume;
- `Start`: exit.

The state transition and volume policy live in [`streaming_audio_state.hpp`](../advanced_examples/streaming_audio_state.hpp) so host tests can exercise them without pretending to decode or play audio.

## Build and run

The pinned dependency set is kos-ports commit `3f5aa6587e79f2e6263da4581651d5ec6ea75a69`. The verified link libraries are `oggvorbisplay`, `vorbis`, and `ogg`; that revision does not install a separate `libvorbisfile.a`.

```sh
make host-test-streaming-audio
make in-container TARGET=dreamcast-streaming-audio-cdi \
  DC_BUILD_TYPE=Release
```

This produces `build-dreamcast/maishuji-streaming-audio.cdi`. Boot it in Flycast with audio output enabled or on a Dreamcast, then exercise stop, play, restart, and volume. Record the emulator version and audio backend, or the console and capture path, when reporting an audible result.

For the deterministic visual and request-path fixture:

```sh
make in-container TARGET=dreamcast-streaming-audio-scripted-cdi \
  DC_BUILD_TYPE=Release
make flycast-streaming-audio-scripted
```

The scripted build requests stop, play, restart, and a volume increase. Its checker requires the exact guest marker plus a visible playing indicator and volume meter. This proves that the supplied fixture reached the expected application state and that the Vorbis start requests did not report failure. It does **not** capture audio and cannot prove audibility, stereo behavior, uninterrupted playback, or real AICA output.

## Verification boundary

The following evidence was obtained when the lesson was added:

- host tests passed for state transitions, volume clamping, Ogg/Vorbis metadata, duration, checksum, size, and deterministic PCM generation;
- pinned KOS Debug and Release all-target builds passed;
- Release scripted CDI packaging passed;
- the scripted Flycast dashboard and exact runtime marker passed.

No controlled audio capture and no physical Dreamcast listening test was performed. A complete manual listening report should confirm that the loop is audible, stop produces silence, play resumes, restart returns near the beginning, trigger volume changes are perceptible, and the loop runs long enough to reveal obvious gaps or underruns.
