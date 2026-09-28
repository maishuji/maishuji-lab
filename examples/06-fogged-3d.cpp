#include "maishuji/mesh.hpp"
#include "maishuji/pvr.hpp"

#include <array>
#include <cstdint>

#include <kos.h>

namespace {

constexpr int frame_count = 180;
// Keep the rendered window alive long enough for the emulator checker.
constexpr int capture_hold_frames = 900;

maishuji::Status run_frame(maishuji::Pvr &pvr, const maishuji::Mesh &mesh,
                           const maishuji::Camera &camera,
                           const maishuji::Fog &fog,
                           int frame_index) noexcept {
    const float time = static_cast<float>(frame_index) * 0.035f;
    const maishuji::Transform transform{
        {0.0f, 0.0f, 0.0f},
        {time * 0.7f, time, time * 0.35f},
        {1.0f, 1.0f, 1.0f},
    };

    maishuji::Frame frame;
    maishuji::Status status = pvr.begin_frame(frame);
    if(maishuji::failed(status))
        return status;

    maishuji::RenderList list;
    status = frame.begin_list(list, maishuji::List::Opaque);
    if(maishuji::failed(status))
        return status;

    status = list.submit_fogged(
        mesh, camera, transform, {640.0f, 480.0f}, fog);
    if(maishuji::failed(status))
        return status;

    status = list.finish();
    if(maishuji::failed(status))
        return status;

    return frame.finish();
}

} // namespace

int main() {
    const std::array<maishuji::MeshVertex, 8> vertices{
        maishuji::MeshVertex{{-0.9f, -0.9f, -0.9f}, {255, 32, 40, 255}},
        maishuji::MeshVertex{{0.9f, -0.9f, -0.9f}, {255, 32, 40, 255}},
        maishuji::MeshVertex{{0.9f, 0.9f, -0.9f}, {255, 32, 40, 255}},
        maishuji::MeshVertex{{-0.9f, 0.9f, -0.9f}, {255, 32, 40, 255}},
        maishuji::MeshVertex{{-0.9f, -0.9f, 0.9f}, {24, 80, 255, 255}},
        maishuji::MeshVertex{{0.9f, -0.9f, 0.9f}, {24, 80, 255, 255}},
        maishuji::MeshVertex{{0.9f, 0.9f, 0.9f}, {24, 80, 255, 255}},
        maishuji::MeshVertex{{-0.9f, 0.9f, 0.9f}, {24, 80, 255, 255}},
    };
    const std::array<std::uint16_t, 36> indices{
        0, 1, 2, 0, 2, 3,
        4, 6, 5, 4, 7, 6,
        0, 4, 5, 0, 5, 1,
        3, 2, 6, 3, 6, 7,
        0, 3, 7, 0, 7, 4,
        1, 5, 6, 1, 6, 2,
    };
    const maishuji::Mesh mesh{vertices, indices};
    const maishuji::Camera camera{
        {0.0f, 0.0f, 4.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        1.04719755f,
        4.0f / 3.0f,
        0.1f,
        100.0f,
    };

    const maishuji::Fog fog{{6, 8, 24, 255}, 2.5f, 4.75f, true};

    maishuji::Pvr pvr;
    maishuji::Status status = pvr.initialize();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: PVR initialization failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }

    for(int frame = 0; frame < frame_count; ++frame) {
        status = run_frame(pvr, mesh, camera, fog, frame);
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR, "maishuji: fogged 3D frame %d failed: %s\n",
                   frame, maishuji::status_name(status));
            (void)pvr.shutdown();
            return 1;
        }
    }

    dbglog(DBG_NOTICE,
           "maishuji: fogged 3D passed (%d frames; camera-space linear fog)\n",
           frame_count);
    for(int frame = 0; frame < capture_hold_frames; ++frame) {
        status = run_frame(pvr, mesh, camera, fog, 0);
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR,
                   "maishuji: fogged 3D capture hold frame %d failed: %s\n",
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

    return 0;
}
