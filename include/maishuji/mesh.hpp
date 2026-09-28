#pragma once

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
};

struct Mesh {
    std::span<const MeshVertex> vertices{};
    std::span<const std::uint16_t> indices{};
};

} // namespace maishuji
