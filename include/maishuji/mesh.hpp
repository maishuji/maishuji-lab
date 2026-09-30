#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

#include "maishuji/math.hpp"
#include "maishuji/pvr.hpp"

namespace maishuji {

struct Viewport {
    float width = 640.0f;
    float height = 480.0f;
};

struct MeshVertex {
    Vec3 position{};
    Color color{};
    Color offset_color{0, 0, 0, 0};
};

struct TexturedMeshVertex {
    Vec3 position{};
    float u = 0.0f;
    float v = 0.0f;
    Color color{};
    Color offset_color{0, 0, 0, 0};
};

struct Fog {
    Color color{8, 8, 24, 255};
    float start = 2.0f;
    float end = 8.0f;
    bool enabled = false;

    bool valid() const noexcept {
        return !enabled ||
               (std::isfinite(start) && std::isfinite(end) &&
                start >= 0.0f && end > start);
    }

    Color apply(Color source, float camera_depth) const noexcept {
        if(!enabled || !std::isfinite(camera_depth))
            return source;

        const float amount =
            std::clamp((camera_depth - start) / (end - start), 0.0f, 1.0f);
        const auto blend = [amount](std::uint8_t first,
                                    std::uint8_t second) noexcept {
            const float value =
                static_cast<float>(first) +
                (static_cast<float>(second) - static_cast<float>(first)) *
                    amount;
            return static_cast<std::uint8_t>(
                std::clamp(value, 0.0f, 255.0f));
        };

        return {
            blend(source.red, color.red),
            blend(source.green, color.green),
            blend(source.blue, color.blue),
            source.alpha,
        };
    }
};

struct Mesh {
    std::span<const MeshVertex> vertices{};
    std::span<const std::uint16_t> indices{};
};

struct TexturedMesh {
    std::span<const TexturedMeshVertex> vertices{};
    std::span<const std::uint16_t> indices{};
};

} // namespace maishuji
