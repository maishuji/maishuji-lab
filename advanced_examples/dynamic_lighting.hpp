#pragma once

#include "maishuji/mesh.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// Example-local CPU lighting and planar projection, independent of KOS.
namespace dynamic_lighting {

inline constexpr float pi = 3.14159265358979323846f;
inline constexpr float ambient = 0.30f;
inline constexpr float diffuse = 0.70f;
inline constexpr float orbit_radius = 4.0f;
inline constexpr float light_height = 4.5f;
inline constexpr float ground_half_extent = 7.0f;
inline constexpr float shadow_height = 0.003f;
inline constexpr maishuji::Color ground_color{229, 217, 190, 255};
inline constexpr maishuji::Color shadow_color{66, 79, 99, 255};
inline constexpr maishuji::List shadow_list = maishuji::List::Opaque;
inline constexpr maishuji::PrimitiveConfiguration shadow_configuration{
    maishuji::Culling::None, maishuji::DepthCompare::GreaterOrEqual, true, false,
};

// Keep the full light orbit clear of the title bar, not only the car in frame.
inline constexpr maishuji::Camera normal_camera{
    {5.0f, 5.0f, 8.0f}, {0.0f, 1.4f, 0.0f}, {0.0f, 1.0f, 0.0f},
    0.95f, 4.0f / 3.0f, 0.1f, 40.0f,
};

inline bool finite(maishuji::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

struct Facet {
    maishuji::Vec3 normal{};
    maishuji::Vec3 centroid{};
};

inline bool make_facet(maishuji::Vec3 a, maishuji::Vec3 b,
                       maishuji::Vec3 c, Facet &result) noexcept {
    if(!finite(a) || !finite(b) || !finite(c)) return false;
    const auto normal = maishuji::cross(b - a, c - a);
    const float squared_length = maishuji::dot(normal, normal);
    if(!std::isfinite(squared_length) || squared_length <= 1.0e-12f)
        return false;
    const Facet candidate{normal * (1.0f / std::sqrt(squared_length)),
                          (a + b + c) * (1.0f / 3.0f)};
    if(!finite(candidate.centroid)) return false;
    result = candidate;
    return true;
}

inline float lambert(const Facet &facet, maishuji::Vec3 light) noexcept {
    const auto direction = light - facet.centroid;
    const float squared_length = maishuji::dot(direction, direction);
    if(!finite(facet.normal) || !finite(direction) ||
       !std::isfinite(squared_length) || squared_length <= 1.0e-12f)
        return ambient;
    const float facing = maishuji::dot(
        facet.normal, direction * (1.0f / std::sqrt(squared_length)));
    return ambient + diffuse * std::clamp(facing, 0.0f, 1.0f);
}

inline maishuji::Color modulate(maishuji::Color base, float amount) noexcept {
    if(!std::isfinite(amount)) amount = ambient;
    amount = std::clamp(amount, 0.0f, 1.0f);
    return {static_cast<std::uint8_t>(base.red * amount + 0.5f),
            static_cast<std::uint8_t>(base.green * amount + 0.5f),
            static_cast<std::uint8_t>(base.blue * amount + 0.5f), base.alpha};
}

inline maishuji::Vec3 orbit_light(float angle) noexcept {
    return {orbit_radius * std::cos(angle), light_height,
            orbit_radius * std::sin(angle)};
}

// The mathematical receiver is y=0; the rendering bias is applied separately.
// Leave the output untouched on failure, including singular/overflowing rays.
inline bool project_to_ground(maishuji::Vec3 point, maishuji::Vec3 light,
                              maishuji::Vec3 &result) noexcept {
    if(!finite(point) || !finite(light) || point.y < 0.0f ||
       light.y - point.y <= 1.0e-5f)
        return false;
    const float amount = point.y / (light.y - point.y);
    auto candidate = point + (point - light) * amount;
    candidate.y = 0.0f;
    if(!finite(candidate)) return false;
    result = candidate;
    return true;
}

// Storage belongs in static memory on SH-4, not in a frame's stack.
// Every source index becomes its own vertex, retaining UVs and unlit colors.
template<std::size_t IndexCount>
struct FlatCar {
    static_assert(IndexCount > 0 && IndexCount % 3 == 0);
    static_assert(IndexCount <= 65536);
    static constexpr std::size_t triangle_count = IndexCount / 3;
    static constexpr std::size_t vertex_count = IndexCount;

    std::array<maishuji::TexturedMeshVertex, IndexCount> base{};
    std::array<maishuji::TexturedMeshVertex, IndexCount> lit{};
    std::array<maishuji::MeshVertex, IndexCount> shadow{};
    std::array<std::uint16_t, IndexCount> indices{};
    std::array<Facet, triangle_count> facets{};
    float maximum_y = 0.0f;
    bool initialized = false;

    template<std::size_t VertexCount>
    bool initialize(
        const std::array<maishuji::TexturedMeshVertex, VertexCount> &source,
        const std::array<std::uint16_t, IndexCount> &source_indices) noexcept {
        initialized = false;
        maximum_y = 0.0f;
        for(std::size_t i = 0; i < IndexCount; ++i) {
            if(source_indices[i] >= VertexCount) return false;
            const auto &vertex = source[source_indices[i]];
            if(!finite(vertex.position) || vertex.position.y < 0.0f ||
               !std::isfinite(vertex.u) || !std::isfinite(vertex.v))
                return false;
            base[i] = vertex;
            lit[i] = vertex;
            shadow[i] = {vertex.position, shadow_color};
            indices[i] = static_cast<std::uint16_t>(i);
            maximum_y = std::max(maximum_y, vertex.position.y);
        }
        for(std::size_t i = 0; i < triangle_count; ++i) {
            if(!make_facet(base[i * 3].position, base[i * 3 + 1].position,
                           base[i * 3 + 2].position, facets[i]))
                return false;
        }
        initialized = true;
        return true;
    }

    bool update(maishuji::Vec3 light) noexcept {
        if(!initialized || !finite(light) ||
           light.y - maximum_y <= 1.0e-5f)
            return false;
        for(std::size_t triangle = 0; triangle < triangle_count; ++triangle) {
            const float intensity = lambert(facets[triangle], light);
            for(std::size_t corner = 0; corner < 3; ++corner) {
                const std::size_t i = triangle * 3 + corner;
                maishuji::Vec3 projected{};
                if(!project_to_ground(base[i].position, light, projected))
                    return false;
                projected.y = shadow_height;
                shadow[i].position = projected;
                lit[i].color = modulate(base[i].color, intensity);
            }
        }
        return true;
    }
};

} // namespace dynamic_lighting
