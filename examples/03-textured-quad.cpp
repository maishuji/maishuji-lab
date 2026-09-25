#include "maishuji/pvr.hpp"

#include <cstdint>
#include <span>

#include <kos.h>

namespace {

maishuji::TexturedQuad make_quad(float left, float top) noexcept {
    constexpr float width = 176.0f;
    constexpr float height = 176.0f;
    return {
        {left, top, 1.0f, 0.0f, 0.0f},
        {left, top + height, 1.0f, 0.0f, 1.0f},
        {left + width, top, 1.0f, 1.0f, 0.0f},
        {left + width, top + height, 1.0f, 1.0f, 1.0f},
    };
}

maishuji::Status run_frame(
    maishuji::Pvr &pvr, const maishuji::Texture &texture,
    const maishuji::TexturedQuad *quads) noexcept {
    maishuji::Frame frame;
    maishuji::Status status = pvr.begin_frame(frame);
    if(maishuji::failed(status))
        return status;

    const maishuji::List lists[3] = {
        maishuji::List::Opaque,
        maishuji::List::PunchThrough,
        maishuji::List::Translucent,
    };

    for(int index = 0; index < 3; ++index) {
        maishuji::RenderList list;
        status = frame.begin_list(list, lists[index]);
        if(maishuji::failed(status))
            return status;

        status = list.submit(texture, quads[index]);
        if(maishuji::failed(status))
            return status;

        status = list.finish();
        if(maishuji::failed(status))
            return status;
    }

    return frame.finish();
}

} // namespace

int main() {
    alignas(32) std::uint16_t pixels[32 * 32]{};
    for(std::uint16_t y = 0; y < 32; ++y) {
        for(std::uint16_t x = 0; x < 32; ++x) {
            const std::uint16_t block = static_cast<std::uint16_t>(
                ((x / 4) + (y / 4)) % 3);
            const std::uint16_t alpha = block == 0 ? 0 : block == 1 ? 8 : 15;
            const std::uint16_t red = static_cast<std::uint16_t>((x / 4) & 15);
            const std::uint16_t green = static_cast<std::uint16_t>((y / 4) & 15);
            const std::uint16_t blue = static_cast<std::uint16_t>(
                ((x / 4) + (y / 4)) & 15);
            pixels[y * 32 + x] = static_cast<std::uint16_t>(
                (alpha << 12) | (red << 8) | (green << 4) | blue);
        }
    }

    maishuji::Pvr pvr;
    maishuji::Status status = pvr.initialize();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: PVR initialization failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }

    maishuji::Texture texture;
    status = texture.allocate(pvr, 32, 32);
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: texture allocation failed: %s\n",
               maishuji::status_name(status));
        (void)pvr.shutdown();
        return 1;
    }

    const std::span<const std::uint16_t> pixel_data{pixels, 32 * 32};
    status = texture.upload(pixel_data);
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: texture upload failed: %s\n",
               maishuji::status_name(status));
        (void)texture.release();
        (void)pvr.shutdown();
        return 1;
    }

    const maishuji::TexturedQuad quads[3] = {
        make_quad(16.0f, 152.0f),
        make_quad(232.0f, 152.0f),
        make_quad(448.0f, 152.0f),
    };

    constexpr int frames = 600;
    for(int frame = 0; frame < frames; ++frame) {
        status = run_frame(pvr, texture, quads);
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR, "maishuji: textured frame %d failed: %s\n",
                   frame, maishuji::status_name(status));
            (void)texture.release();
            (void)pvr.shutdown();
            return 1;
        }
    }

    status = texture.release();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: texture release failed: %s\n",
               maishuji::status_name(status));
        (void)pvr.shutdown();
        return 1;
    }

    status = pvr.shutdown();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: PVR shutdown failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }

    dbglog(DBG_NOTICE, "maishuji: textured quad passed\n");
    return 0;
}
