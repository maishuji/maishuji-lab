#pragma once

#include "maishuji/pixel.hpp"
#include "maishuji/pvr.hpp"

namespace maishuji {

struct SpriteUv {
    float left = 0.0f;
    float top = 0.0f;
    float right = 1.0f;
    float bottom = 1.0f;
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
