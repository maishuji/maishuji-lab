#include "detail/backend.hpp"

#include <kos.h>
#include <dc/pvr.h>

#include <cstddef>

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

    if(pvr_init(&params) < 0)
        return false;

    pvr_set_bg_color(0.02f, 0.02f, 0.06f);
    vid_set_enabled(1);
    return true;
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

pvr_cull_mode_t to_kos_culling(Culling culling) noexcept {
    switch(culling) {
    case Culling::None:
        return PVR_CULLING_NONE;
    case Culling::Clockwise:
        return PVR_CULLING_CW;
    case Culling::CounterClockwise:
        return PVR_CULLING_CCW;
    }

    return PVR_CULLING_NONE;
}

void fill_vertex(pvr_vertex_t &destination, std::uint32_t flags,
                 const Vertex &source) noexcept {
    destination.flags = flags;
    destination.x = source.x;
    destination.y = source.y;
    destination.z = source.z;
    destination.u = 0.0f;
    destination.v = 0.0f;
    destination.argb = source.color.argb();
    destination.oargb = 0;
}

bool submit_colored(List list, const Vertex *vertices, std::size_t count,
                    const PrimitiveConfiguration &configuration) noexcept {
    if(count == 0 || count > 4)
        return false;

    alignas(32) pvr_poly_hdr_t header;
    alignas(32) pvr_vertex_t packet[4];

    pvr_poly_cxt_t context;
    pvr_poly_cxt_col(&context, to_kos_list(list));
    context.gen.shading = PVR_SHADE_GOURAUD;
    context.gen.culling = to_kos_culling(configuration.culling);
    pvr_poly_compile(&header, &context);

    for(std::size_t index = 0; index < count; ++index) {
        const std::uint32_t flags =
            index + 1 == count ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        fill_vertex(packet[index], flags, vertices[index]);
    }

    if(pvr_prim(&header, sizeof(header)) < 0)
        return false;

    for(std::size_t index = 0; index < count; ++index) {
        if(pvr_prim(&packet[index], sizeof(packet[index])) < 0)
            return false;
    }

    return true;
}

bool submit_triangle(List list, const Triangle &triangle,
                     const PrimitiveConfiguration &configuration) noexcept {
    const Vertex vertices[3] = {
        triangle.first,
        triangle.second,
        triangle.third,
    };
    return submit_colored(list, vertices, 3, configuration);
}

bool submit_quad(List list, const Quad &quad,
                 const PrimitiveConfiguration &configuration) noexcept {
    const Vertex vertices[4] = {
        quad.top_left,
        quad.bottom_left,
        quad.top_right,
        quad.bottom_right,
    };
    return submit_colored(list, vertices, 4, configuration);
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
        submit_triangle,
        submit_quad,
        wait_render_done,
        shutdown,
    };
    return backend;
}

} // namespace maishuji::detail
