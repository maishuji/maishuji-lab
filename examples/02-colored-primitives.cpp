#include "maishuji/pvr.hpp"

#include <kos.h>

namespace {

maishuji::Status run_frame(maishuji::Pvr &pvr,
                           const maishuji::Triangle &triangle,
                           const maishuji::Quad &quad) noexcept {
    maishuji::Frame frame;
    maishuji::Status status = pvr.begin_frame(frame);
    if(maishuji::failed(status))
        return status;

    maishuji::RenderList opaque;
    status = frame.begin_list(opaque, maishuji::List::Opaque);
    if(maishuji::failed(status))
        return status;

    status = opaque.submit(triangle);
    if(maishuji::failed(status))
        return status;

    status = opaque.submit(quad);
    if(maishuji::failed(status))
        return status;

    status = opaque.finish();
    if(maishuji::failed(status))
        return status;

    return frame.finish();
}

} // namespace

int main() {
    const maishuji::Triangle triangle{
        {320.0f, 88.0f, 1.0f, {255, 64, 64, 255}},
        {88.0f, 392.0f, 1.0f, {64, 255, 64, 255}},
        {552.0f, 392.0f, 1.0f, {64, 128, 255, 255}},
    };
    const maishuji::Quad quad{
        {120.0f, 120.0f, 1.0f, {255, 255, 255, 255}},
        {120.0f, 300.0f, 1.0f, {255, 128, 64, 255}},
        {300.0f, 120.0f, 1.0f, {128, 192, 255, 255}},
        {300.0f, 300.0f, 1.0f, {255, 64, 192, 255}},
    };

    maishuji::Pvr pvr;
    maishuji::Status status = pvr.initialize();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: PVR initialization failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }

    constexpr int frames = 600;
    for(int frame = 0; frame < frames; ++frame) {
        status = run_frame(pvr, triangle, quad);
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR, "maishuji: colored primitive frame %d failed: %s\n",
                   frame, maishuji::status_name(status));
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

    dbglog(DBG_NOTICE, "maishuji: colored primitives passed\n");
    return 0;
}
