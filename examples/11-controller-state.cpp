#include "input_display.hpp"

#include "maishuji/input.hpp"
#include "maishuji/pvr.hpp"

#include <kos.h>

#ifndef MAISHUJI_INPUT_SCRIPTED
#define MAISHUJI_INPUT_SCRIPTED 0
#endif

namespace {
constexpr bool scripted = MAISHUJI_INPUT_SCRIPTED == 1;

maishuji::ControllerSnapshot scripted_sample(int frame) noexcept {
    maishuji::ControllerSnapshot state{};
    if(frame < 60) return state; // Disconnected, not a hidden default controller.
    if(frame >= 120 && frame < 150) {
        state.availability = maishuji::InputAvailability::Unavailable;
        return state;
    }
    state.availability = maishuji::InputAvailability::Valid;
    state.capabilities = {maishuji::button_mask(maishuji::Button::A) |
                          maishuji::button_mask(maishuji::Button::Start),
                          true, true, true, true};
    state.buttons = maishuji::button_mask(maishuji::Button::A);
    state.stick_x = 96;
    state.stick_y = -64;
    state.left_trigger = 128;
    state.right_trigger = 255;
    return state;
}

bool correct_fixture(const maishuji::ControllerSnapshot &state) noexcept {
    return state.valid() && state.held(maishuji::Button::A) &&
           !state.held(maishuji::Button::Start) &&
           state.stick_x == 96 && state.stick_y == -64 &&
           state.left_trigger == 128 && state.right_trigger == 255;
}
} // namespace

int main() {
    maishuji::Pvr pvr;
    auto status = pvr.initialize();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: controller state PVR init failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }
    maishuji::Texture text;
    status = text.allocate(pvr, input_display::text_width, input_display::text_height);
    if(maishuji::failed(status)) {
        (void)pvr.shutdown();
        return 1;
    }

    int frame = 0;

    while(maishuji::succeeded(status)) {
        const auto sample = scripted ? scripted_sample(frame)
                                     : maishuji::sample_controller();
        if(!scripted && sample.held(maishuji::Button::Start)) break;
        if(scripted && frame >= 150 && !correct_fixture(sample)) {
            dbglog(DBG_ERROR, "maishuji: controller scripted fixture mismatch\n");
            status = maishuji::Status::InvalidConfiguration;
            break;
        }
        status = input_display::label(pvr, text, sample, "11 STATE", scripted);
        if(maishuji::failed(status)) break;
        status = input_display::dashboard(pvr, text, sample);
        if(maishuji::failed(status)) break;
        ++frame;
        if(frame == 180) {
            const auto wait = pvr.wait_render_done();
            if(maishuji::failed(wait)) { status = wait; break; }
            if(scripted) {
                dbglog(DBG_NOTICE, "maishuji: controller state scripted passed\n");
            } else {
                dbglog(DBG_NOTICE, "maishuji: controller state ready (cached input)\n");
            }

        }
        if(scripted && frame >= 3600) break; // Keep the fixture visible for slow captures.
    }
    const auto wait = pvr.wait_render_done();
    if(maishuji::succeeded(status)) status = wait;
    const auto released = text.release();
    if(maishuji::succeeded(status)) status = released;
    const auto shutdown = pvr.shutdown();
    if(maishuji::succeeded(status)) status = shutdown;
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: controller state failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }

    return 0;
}
