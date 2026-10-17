#pragma once

#include "maishuji/input.hpp"
#include "maishuji/pvr.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <span>

#include <dc/biosfont.h>

namespace input_display {
constexpr std::uint16_t text_width = 512;
constexpr std::uint16_t text_height = 128;
constexpr maishuji::Color backdrop{12, 25, 40, 255};
constexpr maishuji::Color panel{38, 64, 82, 255};
constexpr maishuji::Color inactive{82, 108, 124, 255};
constexpr maishuji::Color active{101, 230, 159, 255};
constexpr maishuji::Color accent{255, 196, 94, 255};

constexpr maishuji::Quad box(float x, float y, float w, float h,
                            maishuji::Color color, float depth = 0.6f) noexcept {
    return {{x, y, depth, color}, {x, y + h, depth, color},
            {x + w, y, depth, color}, {x + w, y + h, depth, color}};
}

inline maishuji::Status label(maishuji::Pvr &pvr, maishuji::Texture &texture,
                              const maishuji::ControllerSnapshot &state,
                              const char *lesson, bool scripted) noexcept {
    alignas(32) static std::array<std::uint16_t, text_width * text_height> pixels{};
    pixels.fill(0);
    char lines[256]{};
    ::snprintf(lines, sizeof(lines),
        "%s / CONTROLLER  %s\nPORT A: %s\nA:%d B:%d X:%d START:%d\nX:%+4d Y:%+4d   LT:%3u RT:%3u\n%s",
        lesson, scripted ? "SCRIPTED / NOT LIVE" : "LIVE",
        maishuji::input_availability_name(state.availability),
        state.held(maishuji::Button::A), state.held(maishuji::Button::B),
        state.held(maishuji::Button::X), state.held(maishuji::Button::Start),
        state.stick_x, state.stick_y,
        static_cast<unsigned>(state.left_trigger),
        static_cast<unsigned>(state.right_trigger),
                scripted ? "FIXTURE / NO MAPLE INPUT" : "START: EXIT / CACHED STATE");
    bfont_set_encoding(BFONT_CODE_ISO8859_1);
    bfont_draw_str_ex(pixels.data(), text_width, 0xffff, 0, 16, false, lines);
    // The preceding submitted frame may still reference this PVR allocation.
    auto status = pvr.wait_render_done();
    if(maishuji::failed(status)) return status;
    return texture.upload(std::span<const std::uint16_t>{pixels});
}

inline maishuji::Status dashboard(maishuji::Pvr &pvr,
                                   const maishuji::Texture &text,
                                   const maishuji::ControllerSnapshot &state) noexcept {
    maishuji::Frame frame;
    auto status = pvr.begin_frame(frame);
    if(maishuji::failed(status)) return status;
    maishuji::RenderList shapes;
    status = frame.begin_list(shapes, maishuji::List::Opaque);
    if(maishuji::failed(status)) return status;
    auto put = [&shapes](maishuji::Quad quad) { return shapes.submit(quad); };
    status = put(box(0, 0, 640, 480, backdrop, 0.05f));
    if(maishuji::failed(status)) return status;
    status = put(box(36, 176, 568, 251, panel, 0.4f));
    if(maishuji::failed(status)) return status;
    // D-pad and face buttons are laid out like a controller; only supported,
    // held inputs light up. No connection is displayed as neutral, not scripted.
    struct Key { maishuji::Button button; float x, y; };
    constexpr Key keys[] = {
        {maishuji::Button::Up, 146, 220},
        {maishuji::Button::Down, 146, 290},
        {maishuji::Button::Left, 111, 255},
        {maishuji::Button::Right, 181, 255},
        {maishuji::Button::A, 496, 290},
        {maishuji::Button::B, 461, 255},
        {maishuji::Button::X, 426, 290},
        {maishuji::Button::Y, 461, 325},
        {maishuji::Button::Start, 308, 264},
    };
    for(const auto &key : keys) {
        status = put(box(key.x, key.y, 25, 25,
                         state.held(key.button) ? active : inactive));
        if(maishuji::failed(status)) return status;
    }
    const float x = 240.0f + (state.capabilities.stick_x ? state.stick_x / 128.0f : 0.0f) * 28.0f;
    const float y = 362.0f + (state.capabilities.stick_y ? state.stick_y / 128.0f : 0.0f) * 28.0f;
    status = put(box(206, 328, 68, 68, inactive));
    if(maishuji::failed(status)) return status;
    status = put(box(x - 7, y - 7, 14, 14, state.valid() ? accent : panel, 0.7f));
    if(maishuji::failed(status)) return status;
    constexpr struct Bar { float x; bool left; } bars[] = {{68, true}, {542, false}};
    for(const auto &bar : bars) {
        status = put(box(bar.x, 188, 22, 115, inactive));
        if(maishuji::failed(status)) return status;
        const auto value = bar.left ? state.left_trigger : state.right_trigger;
        const float height = state.valid() ? 115.0f * value / 255.0f : 0.0f;
        if(height > 0) {
            status = put(box(bar.x, 303 - height, 22, height, accent, 0.7f));
            if(maishuji::failed(status)) return status;
        }
    }
    status = shapes.finish();
    if(maishuji::failed(status)) return status;
    maishuji::RenderList glyphs;
    status = frame.begin_list(glyphs, maishuji::List::PunchThrough);
    if(maishuji::failed(status)) return status;
    status = glyphs.submit(text, maishuji::TexturedQuad{
        {24, 18, 1, 0, 0}, {24, 146, 1, 0, 1},
        {536, 18, 1, 1, 0}, {536, 146, 1, 1, 1},
    });
    if(maishuji::failed(status)) return status;
    status = glyphs.finish();
    if(maishuji::failed(status)) return status;
    return frame.finish();
}

} // namespace input_display
