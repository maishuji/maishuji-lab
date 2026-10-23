#pragma once

#include <cstdint>

namespace streaming_audio {

enum class PlaybackState : std::uint8_t {
    Ready,
    Playing,
    Stopped,
    Error,
};

enum class Command : std::uint8_t {
    None,
    Play,
    Stop,
    Restart,
};

struct Input {
    bool play_pressed{};
    bool stop_pressed{};
    bool restart_pressed{};
    std::uint8_t left_trigger{};
    std::uint8_t right_trigger{};
};

struct Update {
    Command command{};
    bool volume_changed{};
};

/// Stores the user-facing state for the standalone streaming-audio lesson.
class State {
public:
    [[nodiscard]] Update update(const Input &input) noexcept {
        const int delta = static_cast<int>(input.right_trigger) / 32 -
                          static_cast<int>(input.left_trigger) / 32;
        const int next_volume = clamp_volume(static_cast<int>(volume_) + delta);
        const bool volume_changed = next_volume != volume_;
        volume_ = next_volume;

        Command command = Command::None;
        if(input.stop_pressed) command = Command::Stop;
        else if(input.restart_pressed) command = Command::Restart;
        else if(input.play_pressed) command = Command::Play;
        return {command, volume_changed};
    }

    void mark_playing() noexcept { playback_ = PlaybackState::Playing; }
    void mark_stopped() noexcept { playback_ = PlaybackState::Stopped; }
    void mark_error() noexcept { playback_ = PlaybackState::Error; }

    [[nodiscard]] PlaybackState playback() const noexcept { return playback_; }
    [[nodiscard]] int volume() const noexcept { return volume_; }

private:
    static constexpr int clamp_volume(int value) noexcept {
        return value < 0 ? 0 : (value > 255 ? 255 : value);
    }

    PlaybackState playback_{PlaybackState::Ready};
    int volume_{180};
};

constexpr const char *playback_name(PlaybackState state) noexcept {
    switch(state) {
        case PlaybackState::Ready: return "READY";
        case PlaybackState::Playing: return "PLAYING";
        case PlaybackState::Stopped: return "STOPPED";
        case PlaybackState::Error: return "ERROR";
    }
    return "UNKNOWN";
}

} // namespace streaming_audio
