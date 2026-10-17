#pragma once

#include "maishuji/input.hpp"

#include <algorithm>
#include <cstdint>

namespace maishuji::detail {

// KOS's cooked cont_state_t fields. The adapter copies them while protecting
// Maple-owned memory; this portable mapping never sees a borrowed KOS pointer.
struct CookedController {
    std::uint32_t buttons = 0;
    int stick_x = 0;
    int stick_y = 0;
    int left_trigger = 0;
    int right_trigger = 0;
};

constexpr ControllerCapabilities decode_capabilities(std::uint32_t bits) noexcept {
    return {
        static_cast<std::uint16_t>(((bits >> 24) & 0xffu) |
                                   ((bits >> 8) & 0xff00u)),
        (bits & (1u << 10)) != 0,
        (bits & (1u << 11)) != 0,
        (bits & (1u << 9)) != 0,
        (bits & (1u << 8)) != 0,
    };
}

inline ControllerSnapshot map_controller(ControllerPort port,
                                         CookedController raw,
                                         std::uint32_t capabilities) noexcept {
    ControllerSnapshot result{};
    result.port = port;
    result.availability = InputAvailability::Valid;
    result.capabilities = decode_capabilities(capabilities);
    result.buttons = static_cast<std::uint16_t>(raw.buttons) & result.capabilities.buttons;
    if(result.capabilities.stick_x)
        result.stick_x = static_cast<std::int16_t>(std::clamp(raw.stick_x, -128, 127));
    if(result.capabilities.stick_y)
        result.stick_y = static_cast<std::int16_t>(std::clamp(raw.stick_y, -128, 127));
    if(result.capabilities.left_trigger)
        result.left_trigger = static_cast<std::uint8_t>(std::clamp(raw.left_trigger, 0, 255));
    if(result.capabilities.right_trigger)
        result.right_trigger = static_cast<std::uint8_t>(std::clamp(raw.right_trigger, 0, 255));
    return result;
}

} // namespace maishuji::detail
