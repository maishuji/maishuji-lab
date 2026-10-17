#include "maishuji/input.hpp"
#include "input_backend.hpp"
#include "detail/controller_mapping.hpp"
#include "../../advanced_examples/camera_controls.hpp"

#include <cstdio>
#include <limits>

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

void test_transitions() {
    using namespace maishuji;
    ButtonTracker tracker;
    ControllerSnapshot sample{};
    sample.availability = InputAvailability::Valid;
    sample.capabilities.buttons = button_mask(Button::A) | button_mask(Button::B);
    sample.buttons = sample.capabilities.buttons;
    auto edges = tracker.update(sample);
    check(edges.held(Button::A) && edges.held(Button::B) &&
          !edges.pressed(Button::A) && !edges.released(Button::B),
          "first valid sample seeds both held buttons without edges");
    edges = tracker.update(sample);
    check(edges.held(Button::A) && !edges.pressed(Button::A),
          "held button does not retrigger");
    sample.buttons = 0;
    edges = tracker.update(sample);
    check(edges.released(Button::A) && edges.released(Button::B) &&
          !edges.held(Button::A), "simultaneous releases appear once");
    edges = tracker.update(sample);
    check(!edges.released(Button::A), "release does not repeat");
    sample.buttons = button_mask(Button::A) | button_mask(Button::B);
    edges = tracker.update(sample);
    check(edges.pressed(Button::A) && edges.pressed(Button::B),
          "simultaneous presses appear once");
    sample.availability = InputAvailability::Disconnected;
    edges = tracker.update(sample);
    check(!edges.held(Button::A) && !edges.released(Button::A),
          "disconnect neutralizes without fabricated releases");
    sample.availability = InputAvailability::Valid;
    edges = tracker.update(sample);
    check(edges.held(Button::A) && !edges.pressed(Button::A),
          "reconnect seeds history without a fabricated press");
    sample.availability = InputAvailability::Unavailable;
    tracker.update(sample);
    sample.availability = InputAvailability::Valid;
    edges = tracker.update(sample);
    check(!edges.pressed(Button::B), "unavailable invalidates history");
    sample.availability = InputAvailability::Unsupported;
    tracker.update(sample);
    sample.availability = InputAvailability::Valid;
    check(!tracker.update(sample).pressed(Button::A), "unsupported invalidates history");
    tracker.reset();
    check(!tracker.update(sample).pressed(Button::A), "explicit reset reseeds history");
    sample.port = ControllerPort::B;
    sample.buttons = 0;
    check(!tracker.update(sample).released(Button::A),
          "switching physical ports seeds without cross-port releases");
}

void test_analog() {
    using namespace maishuji;
    ControllerSnapshot sample{};
    sample.availability = InputAvailability::Valid;
    sample.capabilities = {0, true, true, true, true};
    sample.stick_x = -128;
    sample.stick_y = 127;
    sample.left_trigger = 255;
    const auto unit = normalized_stick(sample);
    check(unit.x == -1.0f && unit.y == 1.0f, "signed endpoints reach unit bounds");
    check(normalized_trigger(sample, true) == 1.0f &&
          normalized_trigger(sample, false) == 0.0f, "triggers scale from byte range");
    sample.stick_x = 127;
    sample.stick_y = -128;
    check(normalized_stick(sample).x == 1.0f && normalized_stick(sample).y == -1.0f,
          "opposite signed endpoints retain orientation");
    AnalogStick output{1.0f, 1.0f};
    check(apply_radial_dead_zone({0, 0}, 0.2f, output) &&
          output.x == 0 && output.y == 0, "center remains neutral");
    check(apply_radial_dead_zone({0.2f, 0}, 0.2f, output) &&
          output.x == 0, "boundary remains neutral");
    check(apply_radial_dead_zone({0.6f, 0}, 0.2f, output) &&
          output.x > 0.49f && output.x < 0.51f, "radial rescaling removes dead zone");
    check(apply_radial_dead_zone({1, 1}, 0.2f, output) &&
          output.x > 0.70f && output.x < 0.72f && output.y == output.x,
          "diagonals clamp magnitude to unit circle");
    check(!apply_radial_dead_zone({1, 0}, -0.1f, output) && output.x == 0 &&
          !apply_radial_dead_zone({1, 0}, 1.0f, output) && output.x == 0,
          "invalid dead zones fail closed");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    check(!apply_radial_dead_zone({nan, 0}, 0.2f, output) && output.x == 0 &&
          !apply_radial_dead_zone({0, 0}, nan, output) && output.y == 0,
          "non-finite analog values and thresholds fail closed");
    sample.availability = InputAvailability::Unavailable;
    check(normalized_stick(sample).x == 0 && normalized_trigger(sample, true) == 0,
          "unavailable input remains neutral");
    sample.availability = InputAvailability::Valid;
    sample.capabilities = {};
    check(normalized_stick(sample).x == 0 && normalized_trigger(sample, true) == 0,
          "unsupported channels remain neutral");
}

void test_camera_controls() {
    using namespace maishuji;
    ControllerSnapshot sample{};
    sample.availability = InputAvailability::Valid;
    sample.capabilities = {button_mask(Button::Up) | button_mask(Button::Right),
                           true, true, false, false};
    sample.buttons = button_mask(Button::Up) | button_mask(Button::Right);
    sample.stick_y = -128;
    CameraPose pose{};
    update_camera_pose(pose, sample, 0.1f);
    check(pose.z < -0.1f && pose.x > 0 && pose.yaw == 0,
          "D-pad and stick movement combine and clamp elapsed time");
    const auto moved = pose;
    update_camera_pose(pose, sample, -1.0f);
    check(pose.x == moved.x && pose.z == moved.z, "invalid elapsed time does not move");
    sample.stick_y = 0;
    sample.buttons = 0;
    sample.stick_x = 127;
    update_camera_pose(pose, sample, 0.05f);
    check(pose.yaw > 0 && pose.yaw < 0.11f, "right stick X turns camera right");
    const auto turned = pose;
    update_camera_pose(pose, sample, std::numeric_limits<float>::infinity());
    check(pose.yaw == turned.yaw, "non-finite time does not rotate");
    sample.availability = InputAvailability::Unavailable;
    update_camera_pose(pose, sample, 0.05f);
    check(pose.x == turned.x && pose.z == turned.z && pose.yaw == turned.yaw,
          "unavailable snapshot never moves camera");
    CameraPose boundary{7.99f, -7.99f, 0};
    sample.availability = InputAvailability::Valid;
    sample.buttons = button_mask(Button::Right) | button_mask(Button::Up);
    sample.stick_x = 0;
    update_camera_pose(boundary, sample, 10.0f);
    check(boundary.x <= 8.0f && boundary.z >= -8.0f,
          "movement stays within lesson floor and single-step time bound");
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
    ControllerSnapshot unsupported{};
    unsupported.availability = InputAvailability::Unsupported;
    host::set_controller_fixture(ControllerPort::A, unsupported);
    ControllerSnapshot unavailable_slot{};
    unavailable_slot.availability = InputAvailability::Unavailable;
    host::set_controller_fixture(ControllerPort::D, unavailable_slot);
    check(sample_controller(ControllerPort::A).availability == InputAvailability::Unsupported &&
          sample_controller(ControllerPort::D).availability == InputAvailability::Unavailable,
          "independent physical slots report distinct availability");
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
    test_transitions();
    test_analog();
    test_camera_controls();
    test_port_isolation();
    if(failures != 0) return 1;
    std::puts("input snapshots: host tests passed");
    return 0;
}
