#pragma once

namespace maishuji {

struct PixelPoint {
    float x = 0.0f;
    float y = 0.0f;
};

constexpr float snap_pixel(float coordinate) noexcept {
    return static_cast<float>(static_cast<int>(
        coordinate + (coordinate >= 0.0f ? 0.5f : -0.5f)));
}

constexpr PixelPoint snap_to_pixel(PixelPoint point) noexcept {
    return {snap_pixel(point.x), snap_pixel(point.y)};
}

struct PixelGrid {
    static constexpr int logical_width = 320;
    static constexpr int logical_height = 240;
    static constexpr int output_width = 640;
    static constexpr int output_height = 480;
    static constexpr int output_scale = 2;

    constexpr PixelPoint snap(PixelPoint logical) const noexcept {
        return snap_to_pixel(logical);
    }

    constexpr PixelPoint to_output(PixelPoint logical) const noexcept {
        const PixelPoint snapped = snap(logical);
        return {snapped.x * static_cast<float>(output_scale),
                snapped.y * static_cast<float>(output_scale)};
    }
};

} // namespace maishuji
