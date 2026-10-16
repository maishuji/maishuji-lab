#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Derived from the authored car_2cv.mtl palette. Opaque, unlit base colors.
// Top-down linear ARGB4444; blue, cream, glass, rubber / metal, canvas, headlamp, taillamp.
namespace maishuji::car_2cv_texture {
inline constexpr std::uint16_t width = 64;
inline constexpr std::uint16_t height = 32;
constexpr std::array<std::uint16_t, width * height> make_pixels() noexcept {
    std::array<std::uint16_t, width * height> result{};
    constexpr std::uint16_t colors[] = {0xf38c, 0xffec, 0xf8cd, 0xf222, 0xfbcd, 0xfdc9, 0xffff, 0xfe43};
    for(std::size_t y = 0; y < height; ++y) {
        for(std::size_t x = 0; x < width; ++x) {
            result[y * width + x] = colors[(y / 16) * 4 + x / 16];
        }
    }
    return result;
}
inline constexpr auto pixels = make_pixels();
} // namespace maishuji::car_2cv_texture
