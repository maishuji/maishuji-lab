#include "maishuji/pvr.hpp"
#include "maishuji/sprite.hpp"

#include <cstdint>
#include <span>

#include <kos.h>

namespace {

constexpr std::uint16_t atlas_width = 32;
constexpr std::uint16_t atlas_height = 16;
constexpr int frame_count = 96;

maishuji::TexturedQuad make_subpixel_quad(
    float left, float top, const maishuji::SpriteUv &uv) noexcept {
    constexpr float size = 48.0f;
    return {
        {left, top, 1.0f, uv.left, uv.top},
        {left, top + size, 1.0f, uv.left, uv.bottom},
        {left + size, top, 1.0f, uv.right, uv.top},
        {left + size, top + size, 1.0f, uv.right, uv.bottom},
    };
}

maishuji::Status run_frame(
    maishuji::Pvr &pvr, maishuji::Texture &texture, int frame_index) noexcept {
    const maishuji::SpriteRegion snapped_region =
        maishuji::SpriteRegion::cell(0, 0, 16, 16);
    const maishuji::SpriteRegion subpixel_region =
        maishuji::SpriteRegion::cell(1, 0, 16, 16);
    const maishuji::SpriteUv snapped_uv =
        snapped_region.normalized(texture.width(), texture.height());
    const maishuji::SpriteUv subpixel_uv =
        subpixel_region.normalized(texture.width(), texture.height());

    const float logical_motion = static_cast<float>(frame_index) * 0.25f;
    const maishuji::Sprite snapped_sprite{
        {40.0f + logical_motion, 80.0f},
        {24.0f, 24.0f},
        snapped_uv,
    };
    const maishuji::TexturedQuad quads[2] = {
        maishuji::make_sprite_quad(snapped_sprite),
        make_subpixel_quad(360.0f + logical_motion * 2.0f,
                           160.0f, subpixel_uv),
    };

    maishuji::Frame frame;
    maishuji::Status status = pvr.begin_frame(frame);
    if(maishuji::failed(status))
        return status;

    maishuji::RenderList list;
    status = frame.begin_list(list, maishuji::List::Opaque);
    if(maishuji::failed(status))
        return status;

    for(const maishuji::TexturedQuad &quad : quads) {
        status = list.submit(texture, quad);
        if(maishuji::failed(status))
            return status;
    }

    status = list.finish();
    if(maishuji::failed(status))
        return status;

    return frame.finish();
}

} // namespace

int main() {
    alignas(32) std::uint16_t pixels[atlas_width * atlas_height]{};
    for(std::uint16_t y = 0; y < atlas_height; ++y) {
        for(std::uint16_t x = 0; x < atlas_width; ++x) {
            const std::uint16_t cell = x / 16;
            const std::uint16_t local_x = static_cast<std::uint16_t>(x % 16);
            const std::uint16_t local_y = static_cast<std::uint16_t>(y % 16);
            const std::uint16_t red = cell == 0
                ? 15
                : static_cast<std::uint16_t>((local_x / 2) & 15);
            const std::uint16_t green = cell == 0
                ? static_cast<std::uint16_t>((local_y / 2) & 15)
                : 15;
            const std::uint16_t blue = cell == 0
                ? static_cast<std::uint16_t>(((local_x + local_y) / 2) & 15)
                : 15;
            pixels[y * atlas_width + x] = static_cast<std::uint16_t>(
                (15u << 12) | (red << 8) | (green << 4) | blue);
        }
    }

    const std::span<const std::uint16_t> pixel_data{
        pixels, atlas_width * atlas_height};

    maishuji::Pvr pvr;
    maishuji::Status status = pvr.initialize();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: PVR initialization failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }

    maishuji::Texture texture;
    status = texture.allocate(pvr, atlas_width, atlas_height);
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: atlas allocation failed: %s\n",
               maishuji::status_name(status));
        (void)pvr.shutdown();
        return 1;
    }

    status = texture.upload(pixel_data);
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: atlas upload failed: %s\n",
               maishuji::status_name(status));
        (void)texture.release();
        (void)pvr.shutdown();
        return 1;
    }

    for(int frame = 0; frame < frame_count; ++frame) {
        status = run_frame(pvr, texture, frame);
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR, "maishuji: pixel-sprite frame %d failed: %s\n",
                   frame, maishuji::status_name(status));
            (void)texture.release();
            (void)pvr.shutdown();
            return 1;
        }
    }

    status = texture.release();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: atlas release failed: %s\n",
               maishuji::status_name(status));
        if(texture.allocated())
            (void)texture.release();
        (void)pvr.shutdown();
        return 1;
    }

    status = pvr.shutdown();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: PVR shutdown failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }

    dbglog(DBG_NOTICE,
           "maishuji: pixel sprites passed (%d frames; snapped versus subpixel)\n",
           frame_count);
    return 0;
}
