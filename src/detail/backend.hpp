#pragma once

#include "maishuji/pvr.hpp"

namespace maishuji::detail {

using TextureHandle = std::uintptr_t;

struct Backend {
    bool (*initialize)(const Configuration &configuration) noexcept;
    bool (*wait_ready)() noexcept;
    bool (*scene_begin)() noexcept;
    bool (*scene_finish)() noexcept;
    bool (*list_begin)(List list) noexcept;
    bool (*list_finish)() noexcept;
    bool (*submit_triangle)(List list, const Triangle &triangle,
                            const PrimitiveConfiguration &configuration) noexcept;
    bool (*submit_quad)(List list, const Quad &quad,
                        const PrimitiveConfiguration &configuration) noexcept;
    bool (*texture_allocate)(std::size_t bytes,
                             TextureHandle &handle) noexcept;
    bool (*texture_upload)(TextureHandle handle,
                           const std::uint16_t *pixels,
                           std::size_t bytes) noexcept;
    void (*texture_free)(TextureHandle handle) noexcept;
    bool (*submit_textured_quad)(
        List list, TextureHandle handle, std::uint16_t width,
        std::uint16_t height, const TexturedQuad &quad,
        const PrimitiveConfiguration &configuration) noexcept;
    bool (*wait_render_done)() noexcept;
    bool (*shutdown)() noexcept;
};

const Backend &default_backend() noexcept;

} // namespace maishuji::detail
