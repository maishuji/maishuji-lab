#pragma once

#include <cstdint>

#include "maishuji/pixel.hpp"
#include "maishuji/pvr.hpp"

namespace maishuji {

struct SpriteUv {
    float left = 0.0f;
    float top = 0.0f;
    float right = 1.0f;
    float bottom = 1.0f;
};

struct SpriteRegion {
    std::uint16_t left = 0;
    std::uint16_t top = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;

    constexpr SpriteUv normalized(std::uint16_t texture_width,
                                   std::uint16_t texture_height) const noexcept {
        if(texture_width == 0 || texture_height == 0)
            return {0.0f, 0.0f, 0.0f, 0.0f};

        const float inverse_width =
            1.0f / static_cast<float>(texture_width);
        const float inverse_height =
            1.0f / static_cast<float>(texture_height);
        const float right = static_cast<float>(left) +
                            static_cast<float>(width);
        const float bottom = static_cast<float>(top) +
                             static_cast<float>(height);

        return {
            static_cast<float>(left) * inverse_width,
            static_cast<float>(top) * inverse_height,
            right * inverse_width,
            bottom * inverse_height,
        };
    }
};

struct Sprite {
    PixelPoint position{};
    PixelPoint size{};
    SpriteUv uv{};
    float z = 1.0f;
    Color color{};
};

constexpr TexturedQuad make_sprite_quad(
    const Sprite &sprite, const PixelGrid &grid = {}) noexcept {
    const PixelPoint top_left = grid.to_output(sprite.position);
    const PixelPoint bottom_right = grid.to_output({
        sprite.position.x + sprite.size.x,
        sprite.position.y + sprite.size.y,
    });

    return {
        {top_left.x, top_left.y, sprite.z, sprite.uv.left, sprite.uv.top,
         sprite.color},
        {top_left.x, bottom_right.y, sprite.z, sprite.uv.left,
         sprite.uv.bottom, sprite.color},
        {bottom_right.x, top_left.y, sprite.z, sprite.uv.right, sprite.uv.top,
         sprite.color},
        {bottom_right.x, bottom_right.y, sprite.z, sprite.uv.right,
         sprite.uv.bottom, sprite.color},
    };
}

} // namespace maishuji
