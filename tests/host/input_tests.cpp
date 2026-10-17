#include "maishuji/input.hpp"
#include "input_backend.hpp"
#include "detail/controller_mapping.hpp"

#include <cstdio>

namespace {
int failures = 0;
void check(bool condition, const char *message) {
    if(!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

void test_mapping() {
    using namespace maishuji;
    constexpr std::uint32_t cap = (1u << 26) | (1u << 27) |
        (1u << 28) | (1u << 8) | (1u << 10);
    const auto sample = detail::map_controller(ControllerPort::B,
        {button_mask(Button::A) | button_mask(Button::Start) | button_mask(Button::X),
         -128, 127, 255, 123}, cap);
    check(sample.valid() && sample.port == ControllerPort::B, "valid copied state keeps port");
    check(sample.held(Button::A) && sample.held(Button::Start), "pressed cooked bits stay high");
    check(!sample.held(Button::X) && sample.buttons ==
        (button_mask(Button::A) | button_mask(Button::Start)), "unsupported button neutral");
    check(sample.stick_x == -128 && sample.stick_y == 0, "supported axis retains signed endpoint");
    check(sample.left_trigger == 0 && sample.right_trigger == 123,
          "capability bits distinguish left and right triggers");
    check(sample.capabilities.buttons & button_mask(Button::Up), "D-pad capability translates to button bit");
    check(!(sample.capabilities.buttons & button_mask(Button::X)), "X capability is a different bit");
    const auto limits = detail::map_controller(ControllerPort::A,
        {0xffffffffu, -999, 999, -10, 999}, 0xffffffffu);
    check(limits.stick_x == -128 && limits.stick_y == 127, "invalid host axes clamp to cooked range");
    check(limits.left_trigger == 0 && limits.right_trigger == 255,
          "invalid host triggers clamp to byte range");
    check(limits.buttons == 0xffffu, "only low sixteen cooked buttons survive");
}

void test_port_isolation() {
    using namespace maishuji;
    host::clear_controller_fixtures();
    check(sample_controller().availability == InputAvailability::Disconnected,
          "default host is disconnected, not simulated input");
    ControllerSnapshot first{};
    first.availability = InputAvailability::Valid;
    first.buttons = button_mask(Button::B);
    first.capabilities.buttons = first.buttons;
    host::set_controller_fixture(ControllerPort::C, first);
    const auto copied = sample_controller(ControllerPort::C);
    check(copied.held(Button::B) && copied.port == ControllerPort::C,
          "physical C has its own owned sample");
    check(sample_controller(ControllerPort::B).availability == InputAvailability::Disconnected,
          "an empty B is not remapped to the first controller C");
    first.buttons = 0;
    check(copied.held(Button::B), "copied state never aliases the fixture");
    ControllerSnapshot unavailable{};
    unavailable.availability = InputAvailability::Unavailable;
    unavailable.buttons = button_mask(Button::B);
    unavailable.capabilities.buttons = unavailable.buttons;
    unavailable.stick_x = 127;
    host::set_controller_fixture(ControllerPort::C, unavailable);
    const auto absent = sample_controller(ControllerPort::C);
    check(!absent.held(Button::B) && absent.buttons == 0 && absent.stick_x == 0 &&
          absent.capabilities.buttons == 0, "unavailable sample is neutral");
    check(sample_controller(static_cast<ControllerPort>(4)).availability ==
              InputAvailability::InvalidPort, "out-of-range port never indexed");
    check(input_availability_name(InputAvailability::Unsupported)[0] == 'U',
          "unavailable/unsupported state labels are readable");
    host::clear_controller_fixtures();
}
} // namespace

int main() {
    test_mapping();
    test_port_isolation();
    if(failures != 0) return 1;
    std::puts("input snapshots: host tests passed");
    return 0;
}
