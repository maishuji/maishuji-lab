#include <kos.h>
#include <dc/pvr.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

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
volatile int global_constructor_runs = 0;
volatile int global_destructor_runs = 0;

struct GlobalRuntimeProbe {
    GlobalRuntimeProbe() {
        global_constructor_runs = 1;
    }

    ~GlobalRuntimeProbe() {
        global_destructor_runs = 1;
        dbglog(DBG_NOTICE, "maishuji-lab: global destructor passed\n");
    }
};

GlobalRuntimeProbe global_runtime_probe;

class MoveOnlyProbe {
public:
    explicit MoveOnlyProbe(int *destructions) noexcept
        : destructions_(destructions) {}

    MoveOnlyProbe(const MoveOnlyProbe &) = delete;
    MoveOnlyProbe &operator=(const MoveOnlyProbe &) = delete;

    MoveOnlyProbe(MoveOnlyProbe &&other) noexcept
        : destructions_(other.destructions_) {
        other.destructions_ = nullptr;
    }

    MoveOnlyProbe &operator=(MoveOnlyProbe &&other) noexcept {
        if(this == &other)
            return *this;

        if(destructions_ != nullptr)
            ++*destructions_;
        destructions_ = other.destructions_;
        other.destructions_ = nullptr;
        return *this;
    }

    ~MoveOnlyProbe() {
        if(destructions_ != nullptr)
            ++*destructions_;
    }

private:
    int *destructions_;
};

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

bool run_runtime_probes() {
    if(global_constructor_runs != 1) {
        dbglog(DBG_ERROR, "maishuji-lab: global constructor probe failed\n");
        return false;
    }

    int destructions = 0;
    {
        MoveOnlyProbe owner(&destructions);
        MoveOnlyProbe moved(std::move(owner));
        (void)moved;
    }
    if(destructions != 1) {
        dbglog(DBG_ERROR, "maishuji-lab: local move-only cleanup probe failed\n");
        return false;
    }

    const std::size_t available_texture_memory = pvr_mem_available();
    const pvr_ptr_t failed_allocation =
        pvr_mem_malloc(available_texture_memory + 32);
    if(failed_allocation != nullptr) {
        pvr_mem_free(failed_allocation);
        dbglog(DBG_ERROR, "maishuji-lab: allocation failure probe unexpectedly succeeded\n");
        return false;
    }

    dbglog(DBG_NOTICE, "maishuji-lab: runtime probes passed\n");
    return true;
}

} // namespace

int main() {
    vid_set_enabled(0);
    vid_set_mode(DM_640x480, PM_RGB565);

    pvr_init_params_t pvr_params = {
        { PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_16,
          PVR_BINSIZE_0, PVR_BINSIZE_16 },
        512 * 1024,
        0, // direct (non-DMA) submission
        0, // FSAA disabled
        0, // translucent autosort enabled
        3, // OPB overflow count
        0  // vertex-buffer double buffering enabled
    };

    if(pvr_init(&pvr_params) < 0) {
        dbglog(DBG_ERROR, "maishuji-lab: PVR initialization failed\n");
        return 1;
    }
    if(!run_runtime_probes()) {
        shutdown_pvr();
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

    constexpr int reference_frames = 600;
    for(int frame = 0; frame < reference_frames; ++frame) {
        if(draw_frame() < 0) {
            dbglog(DBG_ERROR, "maishuji-lab: PVR frame submission failed\n");
            shutdown_pvr();
            return 1;
        }
    }

    if(shutdown_pvr() < 0)
        return 1;
    dbglog(DBG_NOTICE, "maishuji-lab: normal shutdown passed\n");
    return global_destructor_runs == 0 ? 0 : 1;
}
