#include "streaming_audio_state.hpp"

#include "maishuji/input.hpp"
#include "maishuji/pvr.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <span>

#include <dc/biosfont.h>
#include <dc/sound/stream.h>
#include <kos.h>
#include <oggvorbis/sndoggvorbis.h>

#ifndef MAISHUJI_AUDIO_SCRIPTED
#define MAISHUJI_AUDIO_SCRIPTED 0
#endif

namespace {

constexpr bool scripted = MAISHUJI_AUDIO_SCRIPTED == 1;
constexpr char track_path[] = "/rd/streaming-theme.ogg";
constexpr std::uint16_t text_width = 512;
constexpr std::uint16_t text_height = 128;
constexpr maishuji::Color background{10, 22, 38, 255};
constexpr maishuji::Color panel{35, 58, 76, 255};
constexpr maishuji::Color inactive{78, 99, 116, 255};
constexpr maishuji::Color playing{91, 225, 164, 255};
constexpr maishuji::Color accent{255, 188, 76, 255};

class VorbisTrack {
public:
    bool initialize() noexcept {
        if(snd_stream_init() < 0) return false;
        stream_initialized_ = true;
        if(sndoggvorbis_init() < 0) return false;
        decoder_initialized_ = true;
        return true;
    }

    bool play(bool loop, int volume) noexcept {
        if(!decoder_initialized_) return false;
        stop();
        if(sndoggvorbis_start(track_path, loop ? 1 : 0) < 0) return false;
        started_ = true;
        sndoggvorbis_volume(volume);
        return true;
    }

    void stop() noexcept {
        if(!started_) return;
        sndoggvorbis_stop();
        started_ = false;
    }

    void set_volume(int volume) noexcept {
        if(decoder_initialized_) sndoggvorbis_volume(volume);
    }

    [[nodiscard]] bool is_playing() const noexcept {
        return started_ && sndoggvorbis_isplaying() != 0;
    }

    void shutdown() noexcept {
        if(decoder_initialized_) {
            stop();
            sndoggvorbis_shutdown();
            decoder_initialized_ = false;
        }
        if(stream_initialized_) {
            snd_stream_shutdown();
            stream_initialized_ = false;
        }
    }

    ~VorbisTrack() { shutdown(); }

private:
    bool stream_initialized_{};
    bool decoder_initialized_{};
    bool started_{};
};

constexpr maishuji::Quad box(float x, float y, float width, float height,
                             maishuji::Color color, float depth = 0.5f) noexcept {
    return {{x, y, depth, color}, {x, y + height, depth, color},
            {x + width, y, depth, color}, {x + width, y + height, depth, color}};
}

maishuji::ControllerSnapshot fixture(int frame) noexcept {
    maishuji::ControllerSnapshot sample{};
    sample.availability = maishuji::InputAvailability::Valid;
    sample.capabilities = {
        maishuji::button_mask(maishuji::Button::A) |
            maishuji::button_mask(maishuji::Button::B) |
            maishuji::button_mask(maishuji::Button::X),
        false, false, true, true,
    };
    if(frame == 120) sample.buttons = maishuji::button_mask(maishuji::Button::B);
    if(frame == 180) sample.buttons = maishuji::button_mask(maishuji::Button::X);
    if(frame == 240) sample.buttons = maishuji::button_mask(maishuji::Button::A);
    if(frame >= 300 && frame < 360) sample.right_trigger = 255;
    return sample;
}

streaming_audio::Input lesson_input(
    const maishuji::ControllerSnapshot &sample,
    const maishuji::ButtonTransitions &edges) noexcept {
    return {
        edges.pressed(maishuji::Button::X),
        edges.pressed(maishuji::Button::B),
        edges.pressed(maishuji::Button::A),
        sample.left_trigger,
        sample.right_trigger,
    };
}

maishuji::Status draw_frame(maishuji::Pvr &pvr, maishuji::Texture &text,
                            const streaming_audio::State &state,
                            const maishuji::ControllerSnapshot &sample,
                            int frame) noexcept {
    alignas(32) static std::array<std::uint16_t, text_width * text_height> pixels{};
    pixels.fill(0);
    char lines[320]{};
    ::snprintf(lines, sizeof(lines),
               "13 STREAMING AUDIO / %s\n"
               "OGG VORBIS MONO / 22050 Hz / 8 s LOOP\n"
               "STATE:%s  VOLUME:%3d  PORT A:%s\n"
               "A:RESTART  B:STOP  X:PLAY  TRIGGERS:VOLUME\n"
               "%s",
               scripted ? "SCRIPTED INPUT / AUDIO NOT VERIFIED" : "LIVE INPUT",
               streaming_audio::playback_name(state.playback()), state.volume(),
               maishuji::input_availability_name(sample.availability),
               scripted ? "FIXTURE COMMANDS / NOT AN AUDIBILITY TEST" :
                          "START: EXIT / DECODING RUNS ON SH-4");
    bfont_set_encoding(BFONT_CODE_ISO8859_1);
    bfont_draw_str_ex(pixels.data(), text_width, 0xffff, 0, 16, false, lines);
    auto status = pvr.wait_render_done();
    if(maishuji::failed(status)) return status;
    status = text.upload(std::span<const std::uint16_t>{pixels});
    if(maishuji::failed(status)) return status;

    maishuji::Frame render;
    status = pvr.begin_frame(render);
    if(maishuji::failed(status)) return status;
    maishuji::RenderList shapes;
    status = render.begin_list(shapes, maishuji::List::Opaque);
    if(maishuji::failed(status)) return status;
    const maishuji::Color state_color =
        state.playback() == streaming_audio::PlaybackState::Playing ? playing : inactive;
    const float meter = 420.0f * static_cast<float>(state.volume()) / 255.0f;
    const float pulse = state.playback() == streaming_audio::PlaybackState::Playing ?
        30.0f + static_cast<float>((frame / 4) % 12) * 4.0f : 30.0f;
    constexpr maishuji::Quad panels[] = {
        box(0, 0, 640, 480, background, 0.05f),
        box(54, 205, 532, 210, panel, 0.3f),
        box(105, 350, 430, 30, inactive, 0.5f),
    };
    for(const auto &panel_quad : panels) {
        status = shapes.submit(panel_quad);
        if(maishuji::failed(status)) return status;
    }
    status = shapes.submit(box(110, 355, meter, 20, accent, 0.7f));
    if(maishuji::failed(status)) return status;
    status = shapes.submit(box(290, 255, 60, pulse, state_color, 0.7f));
    if(maishuji::failed(status)) return status;
    status = shapes.finish();
    if(maishuji::failed(status)) return status;

    maishuji::RenderList glyphs;
    status = render.begin_list(glyphs, maishuji::List::PunchThrough);
    if(maishuji::failed(status)) return status;
    status = glyphs.submit(text, maishuji::TexturedQuad{
        {24, 18, 1, 0, 0}, {24, 146, 1, 0, 1},
        {536, 18, 1, 1, 0}, {536, 146, 1, 1, 1},
    });
    if(maishuji::failed(status)) return status;
    status = glyphs.finish();
    if(maishuji::failed(status)) return status;
    return render.finish();
}

} // namespace

int main() {
    VorbisTrack track;
    if(!track.initialize()) {
        dbglog(DBG_ERROR, "maishuji: streaming audio initialization failed\n");
        return 1;
    }

    streaming_audio::State state;
    if(!track.play(true, state.volume())) {
        dbglog(DBG_ERROR, "maishuji: Ogg Vorbis start failed (%s)\n", track_path);
        return 1;
    }
    state.mark_playing();

    maishuji::Pvr pvr;
    auto status = pvr.initialize();
    if(maishuji::failed(status)) return 1;
    maishuji::Texture text;
    status = text.allocate(pvr, text_width, text_height);
    if(maishuji::failed(status)) {
        (void)pvr.shutdown();
        return 1;
    }

    maishuji::ButtonTracker tracker;
    int frame = 0;
    while(maishuji::succeeded(status)) {
        const auto sample = scripted ? fixture(frame) :
            maishuji::sample_controller(maishuji::ControllerPort::A);
        const auto edges = tracker.update(sample);
        if(!scripted && edges.held(maishuji::Button::Start)) break;
        const auto update = state.update(lesson_input(sample, edges));
        if(update.volume_changed) track.set_volume(state.volume());
        switch(update.command) {
            case streaming_audio::Command::Play:
                if(track.play(true, state.volume())) state.mark_playing();
                else state.mark_error();
                break;
            case streaming_audio::Command::Stop:
                track.stop();
                state.mark_stopped();
                break;
            case streaming_audio::Command::Restart:
                if(track.play(true, state.volume())) state.mark_playing();
                else state.mark_error();
                break;
            case streaming_audio::Command::None:
                break;
        }
        if(state.playback() == streaming_audio::PlaybackState::Playing &&
           !track.is_playing()) {
            state.mark_error();
        }

        status = draw_frame(pvr, text, state, sample, frame);
        if(maishuji::failed(status)) break;
        ++frame;
        if(scripted && frame == 420) {
            if(state.playback() != streaming_audio::PlaybackState::Playing ||
               state.volume() <= 180) {
                status = maishuji::Status::InvalidConfiguration;
                break;
            }
            const auto wait = pvr.wait_render_done();
            if(maishuji::failed(wait)) { status = wait; break; }
            dbglog(DBG_NOTICE,
                   "maishuji: streaming audio fixture passed (Vorbis requests; audibility unverified)\n");
        }
        if(scripted && frame >= 3600) break;
    }

    track.stop();
    const auto wait = pvr.wait_render_done();
    if(maishuji::succeeded(status)) status = wait;
    const auto released = text.release();
    if(maishuji::succeeded(status)) status = released;
    const auto shutdown = pvr.shutdown();
    if(maishuji::succeeded(status)) status = shutdown;
    track.shutdown();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: streaming audio lesson failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }
    return 0;
}
