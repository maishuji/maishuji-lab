#include "maishuji/pvr.hpp"

#include <array>
#include <cstdint>
#include <span>

#include <dc/biosfont.h>
#include <kos.h>

namespace {

constexpr std::uint16_t texture_width = 256;
constexpr std::uint16_t texture_height = 64;

maishuji::TexturedQuad make_text_quad() noexcept {
    return {
        {64.0f, 160.0f, 1.0f, 0.0f, 0.0f},
        {64.0f, 288.0f, 1.0f, 0.0f, 1.0f},
        {576.0f, 160.0f, 1.0f, 1.0f, 0.0f},
        {576.0f, 288.0f, 1.0f, 1.0f, 1.0f},
    };
}

maishuji::Status run_frame(maishuji::Pvr &pvr,
                           const maishuji::Texture &texture,
                           const maishuji::TexturedQuad &quad) noexcept {
    maishuji::Frame frame;
    maishuji::Status status = pvr.begin_frame(frame);
    if(maishuji::failed(status))
        return status;

    maishuji::RenderList text;
    status = frame.begin_list(text, maishuji::List::PunchThrough);
    if(maishuji::failed(status))
        return status;

    status = text.submit(texture, quad);
    if(maishuji::failed(status))
        return status;

    status = text.finish();
    if(maishuji::failed(status))
        return status;

    return frame.finish();
}

} // namespace

int main() {
    alignas(32) std::array<std::uint16_t,
                           texture_width * texture_height> pixels{};

    // KOS writes 16-bit pixels exactly as supplied when bpp is 16. The
    // project texture contract is ARGB4444, so 0xffff is opaque white and
    // the zero-filled background remains transparent for punch-through.
    bfont_set_encoding(BFONT_CODE_ISO8859_1);
    bfont_draw_str_ex(pixels.data(), texture_width, 0xffff, 0x0000, 16, false,
                      "PVR TEXT\nBIOS FONT");

    maishuji::Pvr pvr;
    maishuji::Status status = pvr.initialize();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: PVR initialization failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }

    maishuji::Texture texture;
    status = texture.allocate(pvr, texture_width, texture_height);
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: text texture allocation failed: %s\n",
               maishuji::status_name(status));
        (void)pvr.shutdown();
        return 1;
    }

    status = texture.upload(
        std::span<const std::uint16_t>{pixels.data(), pixels.size()});
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: text texture upload failed: %s\n",
               maishuji::status_name(status));
        (void)texture.release();
        (void)pvr.shutdown();
        return 1;
    }

    const maishuji::TexturedQuad quad = make_text_quad();
    constexpr int frames = 600;
    for(int frame_index = 0; frame_index < frames; ++frame_index) {
        status = run_frame(pvr, texture, quad);
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR, "maishuji: text frame %d failed: %s\n",
                   frame_index, maishuji::status_name(status));
            (void)texture.release();
            (void)pvr.shutdown();
            return 1;
        }
    }

    status = texture.release();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: text texture release failed: %s\n",
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

    dbglog(DBG_NOTICE, "maishuji: PVR text passed\n");
    return 0;
}
