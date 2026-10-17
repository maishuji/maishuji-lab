#include "input_display.hpp"

#include "maishuji/input.hpp"
#include "maishuji/pvr.hpp"

#include <kos.h>

#ifndef MAISHUJI_INPUT_SCRIPTED
#define MAISHUJI_INPUT_SCRIPTED 0
#endif

namespace {
constexpr bool scripted = MAISHUJI_INPUT_SCRIPTED == 1;
constexpr float dead_zone = 0.20f;

maishuji::ControllerSnapshot fixture(int frame) noexcept {
    maishuji::ControllerSnapshot state{};
    if(frame < 60) return state;
    state.availability = maishuji::InputAvailability::Valid;
    state.capabilities = {0, true, true, true, true};
    state.stick_x = 127;
    state.stick_y = 127;
    state.left_trigger = 128;
    state.right_trigger = 255;
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

    int frame = 0;
    while(maishuji::succeeded(status)) {
        const auto sample = scripted ? fixture(frame) : maishuji::sample_controller();
        if(!scripted && sample.held(maishuji::Button::Start)) break;
        maishuji::AnalogStick processed{};
        if(!maishuji::apply_radial_dead_zone(maishuji::normalized_stick(sample),
                                              dead_zone, processed)) {
            status = maishuji::Status::InvalidConfiguration;
            break;
        }
        status = input_display::label(pvr, text, sample, "13 ANALOG", scripted,
                                      nullptr, 0, 0, &processed);
        if(maishuji::failed(status)) break;
        status = input_display::dashboard(pvr, text, sample, &processed);
        if(maishuji::failed(status)) break;
        ++frame;
        if(scripted && frame == 180) {
            if(processed.x < 0.70f || processed.x > 0.72f ||
               processed.y != processed.x ||
               maishuji::normalized_trigger(sample, false) != 1.0f) {
                dbglog(DBG_ERROR, "maishuji: analog scripted fixture mismatch\n");
                status = maishuji::Status::InvalidConfiguration;
                break;
            }
            const auto wait = pvr.wait_render_done();
            if(maishuji::failed(wait)) { status = wait; break; }
            dbglog(DBG_NOTICE, "maishuji: analog input scripted passed\n");
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
        dbglog(DBG_ERROR, "maishuji: analog input failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }
    return 0;
}
