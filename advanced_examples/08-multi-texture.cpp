#include "maishuji/pvr.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <kos.h>

namespace {

constexpr float screen_width = 640.0f;
constexpr float screen_height = 480.0f;
constexpr int animation_frames = 180;
constexpr int capture_hold_frames = 900;
constexpr std::uint16_t texture_width = 16;
constexpr std::uint16_t texture_height = 16;

constexpr std::uint16_t argb4444(std::uint8_t red, std::uint8_t green,
                                 std::uint8_t blue) noexcept {
    return static_cast<std::uint16_t>(0xf000u |
        ((static_cast<std::uint16_t>(red) & 0x0fu) << 8) |
        ((static_cast<std::uint16_t>(green) & 0x0fu) << 4) |
        (static_cast<std::uint16_t>(blue) & 0x0fu));
}

constexpr std::array<std::uint16_t, texture_width * texture_height>
make_metal_texture() noexcept {
    std::array<std::uint16_t, texture_width * texture_height> pixels{};
    for(std::size_t y = 0; y < texture_height; ++y) {
        for(std::size_t x = 0; x < texture_width; ++x) {
            const bool edge = x == 0 || y == 0 || x == texture_width - 1 ||
                              y == texture_height - 1;
            const bool bolt = (x == 3 || x == 12) && (y == 3 || y == 12);
            pixels[y * texture_width + x] =
                edge ? argb4444(10, 8, 5) :
                bolt ? argb4444(15, 12, 5) : argb4444(11, 10, 8);
        }
    }
    return pixels;
}

constexpr std::array<std::uint16_t, texture_width * texture_height>
make_solar_texture() noexcept {
    std::array<std::uint16_t, texture_width * texture_height> pixels{};
    for(std::size_t y = 0; y < texture_height; ++y) {
        for(std::size_t x = 0; x < texture_width; ++x) {
            const bool grid = x % 4 == 0 || y % 4 == 0;
            pixels[y * texture_width + x] =
                grid ? argb4444(5, 12, 15) : argb4444(1, 4, 10);
        }
    }
    return pixels;
}

constexpr auto body_pixels = make_metal_texture();
constexpr auto solar_pixels = make_solar_texture();

constexpr maishuji::Quad make_background() noexcept {
    constexpr maishuji::Color background{8, 10, 24, 255};
    return {
        {0.0f, 0.0f, 0.05f, background},
        {0.0f, screen_height, 0.05f, background},
        {screen_width, 0.0f, 0.05f, background},
        {screen_width, screen_height, 0.05f, background},
    };
}

constexpr maishuji::TexturedQuad make_region(float left, float top,
                                             float right, float bottom) noexcept {
    constexpr maishuji::Color white{255, 255, 255, 255};
    return {
        {left, top, 0.20f, 0.0f, 0.0f, white},
        {left, bottom, 0.20f, 0.0f, 1.0f, white},
        {right, top, 0.20f, 1.0f, 0.0f, white},
        {right, bottom, 0.20f, 1.0f, 1.0f, white},
    };
}

maishuji::Status run_frame(maishuji::Pvr &pvr,
                           const maishuji::Texture &body_texture,
                           const maishuji::Texture &solar_texture) noexcept {
    maishuji::Frame frame;
    maishuji::Status status = pvr.begin_frame(frame);
    if(maishuji::failed(status))
        return status;

    maishuji::RenderList list;
    status = frame.begin_list(list, maishuji::List::Opaque);
    if(maishuji::failed(status))
        return status;

    status = list.submit(make_background());
    if(maishuji::failed(status))
        return status;
    // Keep material regions separate. The small gaps avoid coplanar edge
    // overlap when the PVR resolves the opaque polygons at the same depth.
    status = list.submit(solar_texture,
                         make_region(80.0f, 205.0f, 236.0f, 275.0f));
    if(maishuji::failed(status))
        return status;
    status = list.submit(body_texture,
                         make_region(244.0f, 175.0f, 396.0f, 305.0f));
    if(maishuji::failed(status))
        return status;
    status = list.submit(solar_texture,
                         make_region(404.0f, 205.0f, 560.0f, 275.0f));
    if(maishuji::failed(status))
        return status;
    status = list.finish();
    if(maishuji::failed(status))
        return status;
    return frame.finish();
}

} // namespace

int main() {
    alignas(32) static auto body_upload = body_pixels;
    alignas(32) static auto solar_upload = solar_pixels;

    maishuji::Pvr pvr;
    maishuji::Status status = pvr.initialize();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: multi-texture PVR initialization failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }

    maishuji::Texture body_texture;
    maishuji::Texture solar_texture;
    status = body_texture.allocate(pvr, texture_width, texture_height);
    if(maishuji::failed(status))
        goto fail_textures;
    status = solar_texture.allocate(pvr, texture_width, texture_height);
    if(maishuji::failed(status))
        goto fail_textures;
    status = body_texture.upload(std::span<const std::uint16_t>{body_upload});
    if(maishuji::failed(status))
        goto fail_textures;
    status = solar_texture.upload(std::span<const std::uint16_t>{solar_upload});
    if(maishuji::failed(status))
        goto fail_textures;

    for(int frame = 0; frame < animation_frames + capture_hold_frames; ++frame) {
        status = run_frame(pvr, body_texture, solar_texture);
        if(maishuji::failed(status))
            goto fail_textures;
        if(frame == animation_frames - 1)
            dbglog(DBG_NOTICE,
                   "maishuji: multi-texture passed (2 textures; satellite body; 2 solar wings; 4 quads; 16x16 ARGB4444)\n");
    }

    status = body_texture.release();
    if(maishuji::failed(status))
        goto fail_pvr;
    status = solar_texture.release();
    if(maishuji::failed(status))
        goto fail_pvr;
    return maishuji::failed(pvr.shutdown()) ? 1 : 0;

fail_textures:
    (void)body_texture.release();
    (void)solar_texture.release();
fail_pvr:
    (void)pvr.shutdown();
    dbglog(DBG_ERROR, "maishuji: multi-texture example failed: %s\n",
           maishuji::status_name(status));
    return 1;
}
