#include "fixed_step.hpp"

#include "maishuji/pvr.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <span>

#include <dc/biosfont.h>
#include <kos.h>

#ifndef MAISHUJI_TIME_SCRIPTED
#define MAISHUJI_TIME_SCRIPTED 0
#endif

namespace {

constexpr bool scripted = MAISHUJI_TIME_SCRIPTED == 1;
constexpr std::uint16_t text_width = 512;
constexpr std::uint16_t text_height = 128;
constexpr maishuji::Color background{11, 24, 40, 255};
constexpr maishuji::Color track{42, 65, 82, 255};
constexpr maishuji::Color variable_color{255, 190, 78, 255};
constexpr maishuji::Color fixed_color{89, 224, 166, 255};

constexpr maishuji::Quad box(float x, float y, float width, float height,
                             maishuji::Color color, float depth = 0.5f) noexcept {
    return {{x, y, depth, color}, {x, y + height, depth, color},
            {x + width, y, depth, color}, {x + width, y + height, depth, color}};
}

std::uint64_t timestamp_for_frame(std::uint32_t frame) noexcept {
    if(!scripted) return timer_us_gettime64();

    // A deterministic rendering fixture: ordinary uneven render intervals plus
    // one explicit stall. It does not represent measured Dreamcast cadence.
    constexpr std::uint64_t intervals[] = {8'000, 25'000, 16'667, 12'000};
    static std::uint64_t timestamp{};
    if(frame > 0) {
        timestamp += frame == 120 ? 250'000 : intervals[(frame - 1) % 4];
    }
    return timestamp;
}

maishuji::Status upload_label(maishuji::Pvr &pvr, maishuji::Texture &texture,
                              const fixed_step::Accumulator &clock,
                              const fixed_step::TickResult &tick,
                              std::uint32_t renders) noexcept {
    alignas(32) static std::array<std::uint16_t, text_width * text_height> pixels{};
    pixels.fill(0);
    char lines[256]{};
    ::snprintf(lines, sizeof(lines),
                  "15 FIXED STEP / %s\n"
                  "RENDERS:%lu  THIS FRAME UPDATES:%lu  DROPPED:%llu\n"
                  "TOTAL FIXED UPDATES:%llu  TOTAL DROPPED:%llu\n"
                  "STEP:16667 us  CATCH-UP LIMIT:4\n"
                  "YELLOW: ELAPSED MOTION  GREEN: FIXED UPDATES",
                  scripted ? "SCRIPTED FIXTURE / NOT LIVE" : "LIVE KOS CLOCK",
                  static_cast<unsigned long>(renders),
                  static_cast<unsigned long>(tick.updates),
                  static_cast<unsigned long long>(tick.dropped_updates),
                  static_cast<unsigned long long>(clock.total_updates()),
                  static_cast<unsigned long long>(clock.total_dropped_updates()));
    bfont_set_encoding(BFONT_CODE_ISO8859_1);
    bfont_draw_str_ex(pixels.data(), text_width, 0xffff, 0, 16, false, lines);
    const auto wait = pvr.wait_render_done();
    if(maishuji::failed(wait)) return wait;
    return texture.upload(std::span<const std::uint16_t>{pixels});
}

maishuji::Status draw_frame(maishuji::Pvr &pvr, const maishuji::Texture &text,
                            float variable_x, float fixed_x) noexcept {
    maishuji::Frame frame;
    auto status = pvr.begin_frame(frame);
    if(maishuji::failed(status)) return status;

    maishuji::RenderList shapes;
    status = frame.begin_list(shapes, maishuji::List::Opaque);
    if(maishuji::failed(status)) return status;
    constexpr maishuji::Quad panels[] = {
        box(0, 0, 640, 480, background, 0.05f),
        box(48, 225, 544, 52, track, 0.3f),
        box(48, 335, 544, 52, track, 0.3f),
    };
    for(const auto &panel : panels) {
        status = shapes.submit(panel);
        if(maishuji::failed(status)) return status;
    }
    status = shapes.submit(box(variable_x, 235, 32, 32, variable_color, 0.7f));
    if(maishuji::failed(status)) return status;
    status = shapes.submit(box(fixed_x, 345, 32, 32, fixed_color, 0.7f));
    if(maishuji::failed(status)) return status;
    status = shapes.finish();
    if(maishuji::failed(status)) return status;

    maishuji::RenderList glyphs;
    status = frame.begin_list(glyphs, maishuji::List::PunchThrough);
    if(maishuji::failed(status)) return status;
    status = glyphs.submit(text, maishuji::TexturedQuad{
        {24, 20, 1, 0, 0}, {24, 148, 1, 0, 1},
        {536, 20, 1, 1, 0}, {536, 148, 1, 1, 1},
    });
    if(maishuji::failed(status)) return status;
    status = glyphs.finish();
    if(maishuji::failed(status)) return status;
    return frame.finish();
}

float wrap_track(float position) noexcept {
    constexpr float start = 52.0f;
    constexpr float length = 504.0f;
    while(position >= start + length) position -= length;
    return position;
}

} // namespace

int main() {
    maishuji::Pvr pvr;
    auto status = pvr.initialize();
    if(maishuji::failed(status)) return 1;

    maishuji::Texture text;
    status = text.allocate(pvr, text_width, text_height);
    if(maishuji::failed(status)) {
        (void)pvr.shutdown();
        return 1;
    }

    fixed_step::Accumulator clock;
    fixed_step::TickResult tick{};
    float variable_x = 52.0f;
    float fixed_x = 52.0f;
    std::uint32_t frame = 0;

    while(maishuji::succeeded(status)) {
        tick = clock.tick(timestamp_for_frame(frame));
        // The elapsed marker follows wall-clock delta, capped only for readable
        // display after a pause. The simulation marker advances fixed increments.
        const auto visual_elapsed = tick.elapsed_us < 100'000 ? tick.elapsed_us : 100'000;
        variable_x = wrap_track(variable_x + static_cast<float>(visual_elapsed) * 0.00012f);
        fixed_x = wrap_track(fixed_x + static_cast<float>(tick.updates) * 2.0f);

        status = upload_label(pvr, text, clock, tick, frame + 1);
        if(maishuji::failed(status)) break;
        status = draw_frame(pvr, text, variable_x, fixed_x);
        if(maishuji::failed(status)) break;
        ++frame;

        if(scripted && frame == 180) {
            const auto wait = pvr.wait_render_done();
            if(maishuji::failed(wait)) { status = wait; break; }
            dbglog(DBG_NOTICE,
                   "maishuji: fixed-step scripted fixture passed (bounded catch-up; timing not measured)\n");
        }
        if(scripted && frame >= 3600) break;
    }

    const auto wait = pvr.wait_render_done();
    if(maishuji::succeeded(status)) status = wait;
    const auto released = text.release();
    if(maishuji::succeeded(status)) status = released;
    const auto shutdown = pvr.shutdown();
    if(maishuji::succeeded(status)) status = shutdown;
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: fixed-step lesson failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }
    return 0;
}
