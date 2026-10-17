#pragma once

#include "maishuji/input.hpp"

#include <algorithm>
#include <cmath>

namespace maishuji {

/// Ground-plane camera coordinates and heading for the input-driven lesson.
struct CameraPose {
    float x = 0;
    float z = 0;
    float yaw = 0;
};

/// Applies one port-A sample using bounded seconds; button-to-action mapping lives here.
inline void update_camera_pose(CameraPose &pose, const ControllerSnapshot &sample,
                               float elapsed_seconds) noexcept {
    if(!sample.valid() || !std::isfinite(elapsed_seconds) || elapsed_seconds <= 0) return;
    const float seconds = std::min(elapsed_seconds, 0.05f);
    AnalogStick stick{};
    if(!apply_radial_dead_zone(normalized_stick(sample), 0.18f, stick)) return;

    const float turn = stick.x * 2.0f * seconds;
    float side = static_cast<float>(sample.held(Button::Right)) -
                 static_cast<float>(sample.held(Button::Left));
    float forward = static_cast<float>(sample.held(Button::Up)) -
                    static_cast<float>(sample.held(Button::Down)) - stick.y;
    const float length = std::sqrt(side * side + forward * forward);
    if(length > 1.0f) {
        side /= length;
        forward /= length;
    }
    pose.yaw += turn;
    const float cosine = std::cos(pose.yaw);
    const float sine = std::sin(pose.yaw);
    constexpr float speed = 3.0f;
    pose.x = std::clamp(pose.x + (cosine * side + sine * forward) * speed * seconds,
                        -8.0f, 8.0f);
    pose.z = std::clamp(pose.z + (sine * side - cosine * forward) * speed * seconds,
                        -8.0f, 8.0f);
}

} // namespace maishuji
