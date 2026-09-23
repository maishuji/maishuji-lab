#include "detail/backend.hpp"

#include <kos.h>
#include <dc/pvr.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

namespace maishuji::detail {
namespace {

int bin_size(bool enabled) noexcept {
    return enabled ? PVR_BINSIZE_16 : PVR_BINSIZE_0;
}

bool initialize(const Configuration &configuration) noexcept {
    vid_set_enabled(0);
    vid_set_mode(DM_640x480, PM_RGB565);

    pvr_init_params_t params = {
        { bin_size(configuration.enable_opaque), PVR_BINSIZE_0,
          bin_size(configuration.enable_translucent), PVR_BINSIZE_0,
          bin_size(configuration.enable_punch_through) },
        static_cast<int>(configuration.vertex_buffer_bytes),
        configuration.vertex_dma ? 1 : 0,
        configuration.fsaa ? 1 : 0,
        configuration.translucent_autosort ? 0 : 1,
        static_cast<int>(configuration.opb_overflow),
        configuration.vertex_buffer_double_buffering ? 0 : 1
    };

    return pvr_init(&params) >= 0;
}

bool wait_ready() noexcept {
    return pvr_wait_ready() >= 0;
}

bool scene_begin() noexcept {
    pvr_scene_begin();
    return true;
}

bool scene_finish() noexcept {
    return pvr_scene_finish() >= 0;
}

pvr_list_t to_kos_list(List list) noexcept {
    switch(list) {
    case List::Opaque:
        return PVR_LIST_OP_POLY;
    case List::PunchThrough:
        return PVR_LIST_PT_POLY;
    case List::Translucent:
        return PVR_LIST_TR_POLY;
    }

    return PVR_LIST_OP_POLY;
}

bool list_begin(List list) noexcept {
    return pvr_list_begin(to_kos_list(list)) >= 0;
}

bool list_finish() noexcept {
    return pvr_list_finish() >= 0;
}

bool wait_render_done() noexcept {
    return pvr_wait_render_done() >= 0;
}

bool shutdown() noexcept {
    const bool result = pvr_shutdown() >= 0;
    vid_set_enabled(0);
    return result;
}

} // namespace

const Backend &default_backend() noexcept {
    static const Backend backend{
        initialize,
        wait_ready,
        scene_begin,
        scene_finish,
        list_begin,
        list_finish,
        wait_render_done,
        shutdown,
    };
    return backend;
}

} // namespace maishuji::detail
