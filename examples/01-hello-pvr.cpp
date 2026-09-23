#include "maishuji/pvr.hpp"

#include <kos.h>

namespace {

maishuji::Status run_frame(maishuji::Pvr &pvr) noexcept {
    maishuji::Frame frame;
    maishuji::Status status = pvr.begin_frame(frame);
    if(maishuji::failed(status))
        return status;

    maishuji::RenderList opaque;
    status = frame.begin_list(opaque, maishuji::List::Opaque);
    if(maishuji::failed(status))
        return status;

    status = opaque.finish();
    if(maishuji::failed(status))
        return status;

    return frame.finish();
}

} // namespace

int main() {
    maishuji::Pvr pvr;
    maishuji::Status status = pvr.initialize();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: PVR initialization failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }

    constexpr int frames = 600;
    for(int frame = 0; frame < frames; ++frame) {
        status = run_frame(pvr);
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR, "maishuji: frame %d failed: %s\n",
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

    dbglog(DBG_NOTICE, "maishuji: hello-pvr lifecycle smoke passed\n");
    return 0;
}
