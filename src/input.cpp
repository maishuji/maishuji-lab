#include "maishuji/input.hpp"

#include <cmath>

namespace maishuji {

AnalogStick normalized_stick(const ControllerSnapshot &sample) noexcept {
    if(!sample.valid()) return {};
    auto axis = [](std::int16_t value) noexcept {
        if(value < -128) value = -128;
        if(value > 127) value = 127;
        return value < 0 ? value / 128.0f : value / 127.0f;
    };
    return {sample.capabilities.stick_x ? axis(sample.stick_x) : 0.0f,
            sample.capabilities.stick_y ? axis(sample.stick_y) : 0.0f};
}

float normalized_trigger(const ControllerSnapshot &sample, bool left) noexcept {
    if(!sample.valid()) return 0;
    if(left) return sample.capabilities.left_trigger ? sample.left_trigger / 255.0f : 0;
    return sample.capabilities.right_trigger ? sample.right_trigger / 255.0f : 0;
}

bool apply_radial_dead_zone(AnalogStick input, float dead_zone,
                            AnalogStick &output) noexcept {
    output = {};
    if(!std::isfinite(dead_zone) || dead_zone < 0 || dead_zone >= 1 ||
       !std::isfinite(input.x) || !std::isfinite(input.y)) return false;
    const float magnitude = std::sqrt(input.x * input.x + input.y * input.y);
    if(!std::isfinite(magnitude)) return false;
    if(magnitude <= dead_zone || magnitude == 0) return true;
    const float clamped = magnitude < 1 ? magnitude : 1;
    const float scale = (clamped - dead_zone) / (1 - dead_zone) / magnitude;
    output = {input.x * scale, input.y * scale};
    return true;
}

const char *input_availability_name(InputAvailability availability) noexcept {
    switch(availability) {
    case InputAvailability::Disconnected: return "DISCONNECTED";
    case InputAvailability::Unsupported: return "UNSUPPORTED";
    case InputAvailability::Unavailable: return "UNAVAILABLE";
    case InputAvailability::Valid: return "VALID (CACHED)";
    case InputAvailability::InvalidPort: return "INVALID PORT";
    }
    return "UNKNOWN";
}

} // namespace maishuji
