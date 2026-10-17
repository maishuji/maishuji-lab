#pragma once

#include <cstdint>

namespace maishuji {

/// Physical top-level Maple controller ports, not enumeration indices.
enum class ControllerPort : std::uint8_t { A, B, C, D };

/// Availability observed at sampling time; Valid does not imply a fresh packet.
enum class InputAvailability : std::uint8_t {
    Disconnected, Unsupported, Unavailable, Valid, InvalidPort,
};

/// Pressed-high button bits in the cooked KOS controller state.
enum class Button : std::uint16_t {
    C = 1u << 0, B = 1u << 1, A = 1u << 2, Start = 1u << 3,
    Up = 1u << 4, Down = 1u << 5, Left = 1u << 6, Right = 1u << 7,
    Z = 1u << 8, Y = 1u << 9, X = 1u << 10, D = 1u << 11,
    Up2 = 1u << 12, Down2 = 1u << 13, Left2 = 1u << 14,
    Right2 = 1u << 15,
};

/// Returns a cooked KOS button mask without querying a device.
constexpr std::uint16_t button_mask(Button button) noexcept {
    return static_cast<std::uint16_t>(button);
}

/// Advertised inputs; unsupported channels are neutral in a valid snapshot.
struct ControllerCapabilities {
    std::uint16_t buttons = 0;
    bool stick_x = false;
    bool stick_y = false;
    bool left_trigger = false;
    bool right_trigger = false;
};

/// Owned copy of a cached Maple controller state for one physical port.
struct ControllerSnapshot {
    ControllerPort port = ControllerPort::A;
    InputAvailability availability = InputAvailability::Disconnected;
    std::uint16_t buttons = 0;
    std::int16_t stick_x = 0;
    std::int16_t stick_y = 0;
    std::uint8_t left_trigger = 0;
    std::uint8_t right_trigger = 0;
    ControllerCapabilities capabilities{};

    constexpr bool valid() const noexcept {
        return availability == InputAvailability::Valid;
    }
    constexpr bool held(Button button) const noexcept {
        const auto mask = button_mask(button);
        return valid() && (capabilities.buttons & mask) != 0 &&
               (buttons & mask) != 0;
    }
};

/// Button bits for one update. Edges require two consecutive valid samples.
struct ButtonTransitions {
    std::uint16_t held_buttons = 0;
    std::uint16_t pressed_buttons = 0;
    std::uint16_t released_buttons = 0;

    constexpr bool held(Button button) const noexcept {
        return (held_buttons & button_mask(button)) != 0;
    }
    constexpr bool pressed(Button button) const noexcept {
        return (pressed_buttons & button_mask(button)) != 0;
    }
    constexpr bool released(Button button) const noexcept {
        return (released_buttons & button_mask(button)) != 0;
    }
};

/// Tracks edges for one physical controller port across application updates.
/// A lost sample clears history; a valid sample after loss seeds it without edges.
class ButtonTracker {
public:
    /// Consumes one owned sample; never fabricates presses or releases on loss.
    ButtonTransitions update(const ControllerSnapshot &sample) noexcept {
        if(!sample.valid()) {
            reset();
            return {};
        }
        if(has_previous_ && sample.port != port_) reset();
        const auto held = static_cast<std::uint16_t>(sample.buttons & sample.capabilities.buttons);
        ButtonTransitions result{held, 0, 0};
        if(has_previous_) {
            result.pressed_buttons = static_cast<std::uint16_t>(held & ~previous_);
            result.released_buttons = static_cast<std::uint16_t>(previous_ & ~held);
        }
        previous_ = held;
        port_ = sample.port;
        has_previous_ = true;
        return result;
    }

    /// Discards previous history before changing port or replay source.
    void reset() noexcept {
        previous_ = 0;
        has_previous_ = false;
    }

private:
    std::uint16_t previous_ = 0;
    ControllerPort port_ = ControllerPort::A;
    bool has_previous_ = false;
};

/// Copies the latest cached state for one physical port; never owns a device.
/// Valid is not a freshness, hotplug generation, or real-hardware guarantee.
ControllerSnapshot sample_controller(ControllerPort port = ControllerPort::A) noexcept;

/// Provides a short stable label for a controller's observed availability.
const char *input_availability_name(InputAvailability availability) noexcept;

} // namespace maishuji
