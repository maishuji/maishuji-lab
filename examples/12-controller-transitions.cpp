#include "input_display.hpp"

#include "maishuji/input.hpp"
#include "maishuji/pvr.hpp"

#include <kos.h>

#ifndef MAISHUJI_INPUT_SCRIPTED
#define MAISHUJI_INPUT_SCRIPTED 0
#endif

namespace {
constexpr bool scripted = MAISHUJI_INPUT_SCRIPTED == 1;

maishuji::ControllerSnapshot fixture(int frame) noexcept {
    maishuji::ControllerSnapshot state{};
    if(frame < 60) return state;
    if(frame >= 180 && frame < 210) {
        state.availability = maishuji::InputAvailability::Unavailable;
        return state;
    }
    state.availability = maishuji::InputAvailability::Valid;
    state.capabilities.buttons = maishuji::button_mask(maishuji::Button::A) |
                                 maishuji::button_mask(maishuji::Button::B);
    if(frame < 90 || (frame >= 120 && frame < 180)) {
        state.buttons = maishuji::button_mask(maishuji::Button::A);
    } else if((frame >= 210 && frame < 240) || frame >= 270) {
        state.buttons = maishuji::button_mask(maishuji::Button::B);
    }
    return state;
}
} // namespace

int main() {
    maishuji::Pvr pvr;
    auto status = pvr.initialize();
    if(maishuji::failed(status)) return 1;
    maishuji::Texture text;
    status = text.allocate(pvr, input_display::text_width, input_display::text_height);
    if(maishuji::failed(status)) {
        (void)pvr.shutdown();
        return 1;
    }

    maishuji::ButtonTracker tracker;
    unsigned a_presses = 0;
    unsigned b_presses = 0;
    int frame = 0;
    while(maishuji::succeeded(status)) {
        const auto sample = scripted ? fixture(frame) : maishuji::sample_controller();
        const auto edges = tracker.update(sample);
        if(!scripted && edges.held(maishuji::Button::Start)) break;
        if(edges.pressed(maishuji::Button::A)) ++a_presses;
        if(edges.pressed(maishuji::Button::B)) ++b_presses;

        status = input_display::label(pvr, text, sample, "12 TRANSITIONS", scripted,
                                      &edges, a_presses, b_presses);
        if(maishuji::failed(status)) break;
        status = input_display::dashboard(pvr, text, sample);
        if(maishuji::failed(status)) break;
        ++frame;
        if(scripted && frame == 300) {
            if(a_presses != 1 || b_presses != 1) {
                dbglog(DBG_ERROR, "maishuji: transition scripted counters wrong\n");
                status = maishuji::Status::InvalidConfiguration;
                break;
            }
            const auto wait = pvr.wait_render_done();
            if(maishuji::failed(wait)) { status = wait; break; }
            dbglog(DBG_NOTICE, "maishuji: controller transitions scripted passed\n");
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
        dbglog(DBG_ERROR, "maishuji: controller transitions failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }
    return 0;
}
