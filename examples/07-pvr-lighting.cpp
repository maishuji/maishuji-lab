#include "maishuji/pvr.hpp"

#include <kos.h>

namespace {

maishuji::Status run_frame(maishuji::Pvr &pvr,
                           const maishuji::Quad &base_quad,
                           const maishuji::Quad &lit_quad,
                           const maishuji::PrimitiveConfiguration &lighting) noexcept {
    maishuji::Frame frame;
    maishuji::Status status = pvr.begin_frame(frame);
    if(maishuji::failed(status))
        return status;

    maishuji::RenderList opaque;
    status = frame.begin_list(opaque, maishuji::List::Opaque);
    if(maishuji::failed(status))
        return status;

    status = opaque.submit(base_quad);
    if(maishuji::failed(status))
        return status;

    status = opaque.submit(lit_quad, lighting);
    if(maishuji::failed(status))
        return status;

    status = opaque.finish();
    if(maishuji::failed(status))
        return status;

    return frame.finish();
}

} // namespace

int main() {
    const maishuji::Quad base_quad{
        {70.0f, 140.0f, 1.0f, {48, 48, 64, 255}},
        {70.0f, 340.0f, 1.0f, {24, 32, 48, 255}},
        {290.0f, 140.0f, 1.0f, {64, 48, 48, 255}},
        {290.0f, 340.0f, 1.0f, {48, 24, 32, 255}},
    };
    const maishuji::Quad lit_quad{
        {350.0f, 140.0f, 1.0f, {32, 32, 48, 255}, {128, 64, 0, 255}},
        {350.0f, 340.0f, 1.0f, {24, 32, 32, 255}, {0, 96, 32, 255}},
        {570.0f, 140.0f, 1.0f, {32, 32, 48, 255}, {32, 32, 128, 255}},
        {570.0f, 340.0f, 1.0f, {24, 32, 32, 255}, {64, 0, 96, 255}},
    };
    maishuji::PrimitiveConfiguration lighting{};
    lighting.enable_offset_color = true;

    maishuji::Pvr pvr;
    maishuji::Status status = pvr.initialize();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: PVR initialization failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }

    constexpr int frames = 600;
    for(int frame_index = 0; frame_index < frames; ++frame_index) {
        status = run_frame(pvr, base_quad, lit_quad, lighting);
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR, "maishuji: lighting frame %d failed: %s\n",
                   frame_index, maishuji::status_name(status));
            (void)pvr.shutdown();
            return 1;
        }
    }

    status = pvr.shutdown();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: PVR shutdown failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }

    dbglog(DBG_NOTICE, "maishuji: PVR offset-color lighting passed\n");
    return 0;
}
