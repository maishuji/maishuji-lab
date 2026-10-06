#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
namespace maishuji::human_model_texture {
inline constexpr std::uint16_t width = 64;
inline constexpr std::uint16_t height = 64;
constexpr std::array<std::uint16_t, width * height> make_pixels() noexcept {
    std::array<std::uint16_t, width * height> result{};
    constexpr std::uint16_t colors[] = {0xf39b, 0xf347, 0xfdab, 0xf223};
    for(std::size_t y = 0; y < height; ++y)
        for(std::size_t x = 0; x < width; ++x)
            result[y * width + x] = colors[(y / 32) * 2 + x / 32];
    return result;
}
inline constexpr auto pixels = make_pixels();
} // namespace maishuji::human_model_texture
