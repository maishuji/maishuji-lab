#include <kos.h>
#include <dc/pvr.h>

#include <concepts>
#include <cstdint>
#include <span>

KOS_INIT_FLAGS(INIT_DEFAULT);

template <typename T>
concept ScreenCoordinate = std::floating_point<T>;

template <ScreenCoordinate T>
constexpr T midpoint(T first, T second) {
    return (first + second) / static_cast<T>(2);
}

static_assert(__cplusplus >= 202002L);
static_assert(midpoint(2.0f, 6.0f) == 4.0f);
static_assert(sizeof(pvr_vertex_t) == 32);
static_assert(alignof(pvr_vertex_t) == 32);
static_assert(sizeof(pvr_poly_hdr_t) == 32);
static_assert(alignof(pvr_poly_hdr_t) == 32);

namespace {

alignas(32) pvr_poly_hdr_t triangle_header;
alignas(32) pvr_vertex_t triangle_vertices[3];

void set_vertex(pvr_vertex_t &vertex, std::uint32_t flags, float x, float y,
                std::uint32_t color) {
    vertex.flags = flags;
    vertex.x = x;
    vertex.y = y;
    vertex.z = 1.0f;
    vertex.u = 0.0f;
    vertex.v = 0.0f;
    vertex.argb = color;
    vertex.oargb = 0;
}

int draw_frame() {
    if(pvr_wait_ready() < 0)
        return -1;

    pvr_scene_begin();

    if(pvr_list_begin(PVR_LIST_OP_POLY) < 0)
        return -1;
    if(pvr_prim(&triangle_header, sizeof(triangle_header)) < 0)
        return -1;

    const std::span<const pvr_vertex_t, 3> vertices{triangle_vertices};
    for(const auto &vertex : vertices) {
        if(pvr_prim(&vertex, sizeof(vertex)) < 0)
            return -1;
    }

    if(pvr_list_finish() < 0)
        return -1;
    return pvr_scene_finish();
}

int shutdown_pvr() {
    int status = pvr_wait_render_done();
    if(status < 0)
        dbglog(DBG_ERROR, "maishuji-lab: PVR render wait failed during shutdown\n");

    if(pvr_shutdown() < 0) {
        dbglog(DBG_ERROR, "maishuji-lab: PVR shutdown failed\n");
        status = -1;
    }

    vid_set_enabled(0);
    return status;
}

} // namespace

int main() {
    vid_set_enabled(0);
    vid_set_mode(DM_640x480, PM_RGB565);

    pvr_init_params_t pvr_params = pvr_default_params;
    pvr_params.opb_sizes[0] = PVR_BINSIZE_16; // opaque polygons
    pvr_params.opb_sizes[1] = PVR_BINSIZE_0;  // opaque modifiers
    pvr_params.opb_sizes[2] = PVR_BINSIZE_16; // translucent polygons
    pvr_params.opb_sizes[3] = PVR_BINSIZE_0;  // translucent modifiers
    pvr_params.opb_sizes[4] = PVR_BINSIZE_16; // punch-through polygons
    pvr_params.vertex_buf_size = 512 * 1024;
    pvr_params.dma_enabled = 0;
    pvr_params.fsaa_enabled = 0;
    pvr_params.autosort_disabled = 0;
    pvr_params.opb_overflow_count = 3;
    pvr_params.vbuf_doublebuf_disabled = 0;

    if(pvr_init(&pvr_params) < 0) {
        dbglog(DBG_ERROR, "maishuji-lab: PVR initialization failed\n");
        return 1;
    }
    pvr_set_bg_color(0.02f, 0.02f, 0.06f);
    vid_set_enabled(1);

    pvr_poly_cxt_t context;
    pvr_poly_cxt_col(&context, PVR_LIST_OP_POLY);
    context.gen.shading = PVR_SHADE_GOURAUD;
    // This 2D reference should not disappear based on its vertex winding.
    context.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&triangle_header, &context);

    set_vertex(triangle_vertices[0], PVR_CMD_VERTEX, 320.0f, 88.0f, 0xffff4040);
    set_vertex(triangle_vertices[1], PVR_CMD_VERTEX, 88.0f, 392.0f, 0xff40ff40);
    set_vertex(triangle_vertices[2], PVR_CMD_VERTEX_EOL, 552.0f, 392.0f, 0xff4080ff);

    for(;;) {
        if(draw_frame() < 0) {
            dbglog(DBG_ERROR, "maishuji-lab: PVR frame submission failed\n");
            shutdown_pvr();
            return 1;
        }
    }
}
