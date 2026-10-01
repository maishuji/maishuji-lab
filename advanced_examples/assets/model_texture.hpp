#pragma once

#include <array>
#include <cstdint>

namespace maishuji::model_texture {

inline constexpr std::uint16_t width = 32;
inline constexpr std::uint16_t height = 32;

constexpr std::uint16_t argb4444(std::uint16_t red, std::uint16_t green,
                                 std::uint16_t blue) noexcept {
    return static_cast<std::uint16_t>(
        (0xfu << 12) | ((red & 0xfu) << 8) | ((green & 0xfu) << 4) |
        (blue & 0xfu));
}

constexpr std::uint16_t color_for_tile(std::uint16_t tile) noexcept {
    switch(tile % 4) {
    case 0:
        return argb4444(2, 10, 15);
    case 1:
        return argb4444(12, 3, 15);
    case 2:
        return argb4444(15, 10, 2);
    default:
        return argb4444(3, 15, 8);
    }
}

constexpr std::array<std::uint16_t, width * height> make_pixels() noexcept {
    std::array<std::uint16_t, width * height> result{};
    for(std::uint16_t y = 0; y < height; ++y) {
        for(std::uint16_t x = 0; x < width; ++x) {
            const std::uint16_t tile =
                static_cast<std::uint16_t>((x / 8) + (y / 8));
            result[static_cast<std::size_t>(y) * width + x] =
                color_for_tile(tile);
        }
    }
    return result;
}

inline constexpr auto pixels = make_pixels();

} // namespace maishuji::model_texture
