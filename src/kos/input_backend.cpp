#include "maishuji/input.hpp"
#include "detail/controller_mapping.hpp"

#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <kos/irq.h>

namespace maishuji {
namespace {
// The pinned KOS IRQ assembly lacks a memory clobber. Enclose the copied
// Maple-owned values with compiler barriers as well as the saved IRQ mask.
inline void compiler_barrier() noexcept {
    __asm__ volatile("" ::: "memory");
}

static_assert(CONT_A == button_mask(Button::A));
static_assert(CONT_START == button_mask(Button::Start));
static_assert(CONT_DPAD_UP == button_mask(Button::Up));
static_assert(CONT_DPAD_RIGHT == button_mask(Button::Right));
static_assert(CONT_CAPABILITY_A == (1u << 26));
static_assert(CONT_CAPABILITY_ANALOG_X == (1u << 10));
static_assert(CONT_CAPABILITY_LTRIG == (1u << 9));
}

ControllerSnapshot sample_controller(ControllerPort port) noexcept {
    ControllerSnapshot sample{};
    sample.port = port;
    const int index = static_cast<int>(port);
    if(index < 0 || index >= MAPLE_PORT_COUNT) {
        sample.availability = InputAvailability::InvalidPort;
        return sample;
    }

    detail::CookedController cooked{};
    std::uint32_t capabilities = 0;
    const irq_mask_t previous = irq_disable();
    compiler_barrier();
    // Every device lookup, validation and dereference remains inside this
    // nonblocking interval; no KOS pointer escapes into application code.
    maple_device_t *device = maple_enum_dev(index, 0);
    if(device != nullptr) {
        if((device->info.functions & MAPLE_FUNC_CONTROLLER) == 0) {
            sample.availability = InputAvailability::Unsupported;
        } else {
            const auto *state = static_cast<const cont_state_t *>(maple_dev_status(device));
            if(state == nullptr) {
                sample.availability = InputAvailability::Unavailable;
            } else {
                cooked = {state->buttons, state->joyx, state->joyy,
                          state->ltrig, state->rtrig};
                capabilities = device->info.function_data[0];
                sample.availability = InputAvailability::Valid;
            }
        }
    }
    compiler_barrier();
    irq_restore(previous);
    if(sample.valid()) sample = detail::map_controller(port, cooked, capabilities);
    return sample;
}

} // namespace maishuji
