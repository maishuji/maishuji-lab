#include "streaming_audio_state.hpp"

#include <cstdio>
#include <cstring>

namespace {

bool expect(bool condition, const char *message) {
    if(condition) return true;
    std::fprintf(stderr, "FAIL: %s\n", message);
    return false;
}

} // namespace

int main() {
    bool passed = true;
    streaming_audio::State state;
    passed &= expect(state.playback() == streaming_audio::PlaybackState::Ready &&
                         state.volume() == 180,
                     "initial state is ready at a safe volume");

    auto update = state.update({.play_pressed = true});
    passed &= expect(update.command == streaming_audio::Command::Play,
                     "play edge requests playback");
    state.mark_playing();
    passed &= expect(state.playback() == streaming_audio::PlaybackState::Playing,
                     "successful start marks playback active");

    update = state.update({.play_pressed = true, .stop_pressed = true,
                           .restart_pressed = true});
    passed &= expect(update.command == streaming_audio::Command::Stop,
                     "stop wins when command edges coincide");
    state.mark_stopped();
    passed &= expect(state.playback() == streaming_audio::PlaybackState::Stopped,
                     "stop result is visible");

    update = state.update({.restart_pressed = true});
    passed &= expect(update.command == streaming_audio::Command::Restart,
                     "restart has its own explicit command");

    for(int i = 0; i < 40; ++i) {
        update = state.update({.right_trigger = 255});
    }
    passed &= expect(state.volume() == 255 && !update.volume_changed,
                     "volume clamps at the AICA maximum");
    for(int i = 0; i < 40; ++i) {
        update = state.update({.left_trigger = 255});
    }
    passed &= expect(state.volume() == 0 && !update.volume_changed,
                     "volume clamps at silence");

    const auto neutral = state.update({});
    passed &= expect(neutral.command == streaming_audio::Command::None &&
                         !neutral.volume_changed,
                     "missing or neutral input changes nothing");
    state.mark_error();
    passed &= expect(state.playback() == streaming_audio::PlaybackState::Error &&
                         std::strcmp(streaming_audio::playback_name(state.playback()),
                                     "ERROR") == 0,
                     "runtime failures produce an explicit error state");

    if(!passed) return 1;
    std::puts("Streaming audio state: host tests passed");
    return 0;
}
