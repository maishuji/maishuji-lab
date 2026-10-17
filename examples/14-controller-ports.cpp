#include "input_display.hpp"

#include "maishuji/input.hpp"
#include "maishuji/pvr.hpp"

#include <array>
#include <cstdio>
#include <span>

#include <dc/biosfont.h>
#include <kos.h>

#ifndef MAISHUJI_INPUT_SCRIPTED
#define MAISHUJI_INPUT_SCRIPTED 0
#endif

namespace {
constexpr bool scripted = MAISHUJI_INPUT_SCRIPTED == 1;
constexpr std::uint16_t width = 512;
constexpr std::uint16_t height = 256;
using Ports = std::array<maishuji::ControllerSnapshot, 4>;

Ports sample_ports() noexcept {
    Ports states{};
    for(int i = 0; i < 4; ++i) {
        const auto port = static_cast<maishuji::ControllerPort>(i);
        if(scripted) {
            states[i].port = port;
            states[i].availability = i == 0 ? maishuji::InputAvailability::Valid :
                i == 1 ? maishuji::InputAvailability::Disconnected :
                i == 2 ? maishuji::InputAvailability::Unsupported :
                         maishuji::InputAvailability::Unavailable;
            if(i == 0) {
                states[i].buttons = maishuji::button_mask(maishuji::Button::A);
                states[i].capabilities.buttons = states[i].buttons;
                states[i].stick_x = 64;
                states[i].capabilities.stick_x = true;
            }
        } else {
            states[i] = maishuji::sample_controller(port);
        }
    }
    return states;
}

maishuji::Status draw(maishuji::Pvr &pvr, maishuji::Texture &text,
                      const Ports &ports) noexcept {
    alignas(32) static std::array<std::uint16_t, width * height> pixels{};
    pixels.fill(0);
    bfont_set_encoding(BFONT_CODE_ISO8859_1);
    for(int i = 0; i < 4; ++i) {
        const auto &state = ports[i];
        char line[128]{};
        ::snprintf(line, sizeof(line), "PORT %c: %s%s\nA:%d B:%d X:%+4d LT:%3u RT:%3u",
                   'A' + i, maishuji::input_availability_name(state.availability),
                   scripted ? " / SCRIPTED" : " / LIVE",
                   state.held(maishuji::Button::A), state.held(maishuji::Button::B),
                   state.stick_x, static_cast<unsigned>(state.left_trigger),
                   static_cast<unsigned>(state.right_trigger));
        bfont_draw_str_ex(pixels.data() + i * 64 * width, width, 0xffff,
                          0, 16, false, line);
    }
    auto status = pvr.wait_render_done();
    if(maishuji::failed(status)) return status;
    status = text.upload(std::span<const std::uint16_t>{pixels});
    if(maishuji::failed(status)) return status;

    maishuji::Frame frame;
    status = pvr.begin_frame(frame);
    if(maishuji::failed(status)) return status;
    maishuji::RenderList panels;
    status = frame.begin_list(panels, maishuji::List::Opaque);
    if(maishuji::failed(status)) return status;
    status = panels.submit(input_display::box(0, 0, 640, 480, input_display::backdrop, 0.05f));
    if(maishuji::failed(status)) return status;
    for(int i = 0; i < 4; ++i) {
        const float y = 36.0f + 102.0f * i;
        status = panels.submit(input_display::box(26, y, 588, 94, input_display::panel));
        if(maishuji::failed(status)) return status;
        const auto color = ports[i].valid() ? input_display::active :
            ports[i].availability == maishuji::InputAvailability::Disconnected ?
                input_display::inactive : input_display::accent;
        status = panels.submit(input_display::box(28, y + 4, 8, 86, color, 0.7f));
        if(maishuji::failed(status)) return status;
    }
    status = panels.finish();
    if(maishuji::failed(status)) return status;
    maishuji::RenderList glyphs;
    status = frame.begin_list(glyphs, maishuji::List::PunchThrough);
    if(maishuji::failed(status)) return status;
    status = glyphs.submit(text, maishuji::TexturedQuad{
        {46, 45, 1, 0, 0}, {46, 453, 1, 0, 1},
        {558, 45, 1, 1, 0}, {558, 453, 1, 1, 1},
    });
    if(maishuji::failed(status)) return status;
    status = glyphs.finish();
    if(maishuji::failed(status)) return status;
    return frame.finish();
}
} // namespace

int main() {
    maishuji::Pvr pvr;
    auto status = pvr.initialize();
    if(maishuji::failed(status)) return 1;
    maishuji::Texture text;
    status = text.allocate(pvr, width, height);
    if(maishuji::failed(status)) { (void)pvr.shutdown(); return 1; }
    int frame = 0;
    while(maishuji::succeeded(status)) {
        const auto ports = sample_ports();
        if(!scripted && ports[0].held(maishuji::Button::Start)) break;
        status = draw(pvr, text, ports);
        if(maishuji::failed(status)) break;
        ++frame;
        if(scripted && frame == 180) {
            if(!ports[0].held(maishuji::Button::A) ||
               ports[1].availability != maishuji::InputAvailability::Disconnected ||
               ports[2].availability != maishuji::InputAvailability::Unsupported ||
               ports[3].availability != maishuji::InputAvailability::Unavailable) {
                status = maishuji::Status::InvalidConfiguration;
                break;
            }
            const auto wait = pvr.wait_render_done();
            if(maishuji::failed(wait)) { status = wait; break; }
            dbglog(DBG_NOTICE, "maishuji: controller ports scripted passed\n");
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
        dbglog(DBG_ERROR, "maishuji: controller ports failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }
    return 0;
}
