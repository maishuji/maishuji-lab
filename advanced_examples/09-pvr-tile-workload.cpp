#include "maishuji/pvr.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include <kos.h>

namespace {

constexpr float screen_width = 640.0f;
constexpr float screen_height = 480.0f;
constexpr int warmup_frames = 180;
constexpr int capture_hold_frames = 900;
constexpr int tile_size = 32;
constexpr int tile_columns = static_cast<int>(screen_width) / tile_size;
constexpr int tile_rows = static_cast<int>(screen_height) / tile_size;
constexpr std::size_t workload_quads = 6;

struct TileStats {
    std::uint32_t polygon_count = 0;
    std::uint32_t tile_references = 0;
    std::uint8_t max_tile_layers = 0;
};

struct TileCoverage {
    std::array<std::uint8_t, tile_columns * tile_rows> layers{};
    TileStats stats{};

    void add(const maishuji::Quad &quad) noexcept {
        const float left = std::min({quad.top_left.x, quad.bottom_left.x,
                                     quad.top_right.x, quad.bottom_right.x});
        const float right = std::max({quad.top_left.x, quad.bottom_left.x,
                                      quad.top_right.x, quad.bottom_right.x});
        const float top = std::min({quad.top_left.y, quad.bottom_left.y,
                                    quad.top_right.y, quad.bottom_right.y});
        const float bottom = std::max({quad.top_left.y, quad.bottom_left.y,
                                       quad.top_right.y, quad.bottom_right.y});

        const int first_column = std::clamp(
            static_cast<int>(left) / tile_size, 0, tile_columns - 1);
        const int last_column = std::clamp(
            static_cast<int>(right - 0.001f) / tile_size, 0, tile_columns - 1);
        const int first_row = std::clamp(
            static_cast<int>(top) / tile_size, 0, tile_rows - 1);
        const int last_row = std::clamp(
            static_cast<int>(bottom - 0.001f) / tile_size, 0, tile_rows - 1);

        ++stats.polygon_count;
        for(int row = first_row; row <= last_row; ++row) {
            for(int column = first_column; column <= last_column; ++column) {
                const std::size_t index =
                    static_cast<std::size_t>(row * tile_columns + column);
                const std::uint8_t layer_count = ++layers[index];
                stats.max_tile_layers =
                    std::max(stats.max_tile_layers, layer_count);
                ++stats.tile_references;
            }
        }
    }
};

constexpr maishuji::Quad make_quad(float left, float top, float right,
                                    float bottom, float z,
                                    maishuji::Color color) noexcept {
    return {
        {left, top, z, color},
        {left, bottom, z, color},
        {right, top, z, color},
        {right, bottom, z, color},
    };
}

std::array<maishuji::Quad, workload_quads> make_tile_friendly_workload() noexcept {
    constexpr std::array<maishuji::Color, workload_quads> colors{{
        {70, 180, 110, 255}, {80, 195, 120, 255}, {90, 210, 130, 255},
        {60, 165, 100, 255}, {75, 185, 115, 255}, {95, 220, 140, 255},
    }};
    std::array<maishuji::Quad, workload_quads> quads{};
    for(std::size_t row = 0; row < 2; ++row) {
        for(std::size_t column = 0; column < 3; ++column) {
            const std::size_t index = row * 3 + column;
            const float left = 40.0f + static_cast<float>(column) * 84.0f;
            const float top = 145.0f + static_cast<float>(row) * 105.0f;
            quads[index] = make_quad(left, top, left + 76.0f, top + 92.0f,
                                     0.40f, colors[index]);
        }
    }
    return quads;
}

std::array<maishuji::Quad, workload_quads> make_tile_hostile_workload() noexcept {
    constexpr std::array<maishuji::Color, workload_quads> colors{{
        {220, 80, 70, 255}, {235, 95, 65, 255}, {210, 100, 60, 255},
        {240, 115, 75, 255}, {225, 130, 80, 255}, {245, 145, 90, 255},
    }};
    std::array<maishuji::Quad, workload_quads> quads{};
    for(std::size_t index = 0; index < workload_quads; ++index) {
        const float inset = static_cast<float>(index) * 13.0f;
        quads[index] = make_quad(344.0f + inset, 125.0f + inset,
                                 616.0f - inset, 355.0f - inset,
                                 0.20f + static_cast<float>(index) * 0.03f,
                                 colors[index]);
    }
    return quads;
}

TileStats estimate(std::array<maishuji::Quad, workload_quads> quads) noexcept {
    TileCoverage coverage{};
    for(const maishuji::Quad &quad : quads)
        coverage.add(quad);
    return coverage.stats;
}

constexpr maishuji::Quad make_background() noexcept {
    constexpr maishuji::Color background{8, 10, 24, 255};
    return make_quad(0.0f, 0.0f, screen_width, screen_height, 0.05f,
                     background);
}

constexpr maishuji::Quad make_left_backdrop() noexcept {
    return make_quad(24.0f, 96.0f, 312.0f, 392.0f, 0.10f,
                     {12, 35, 25, 255});
}

constexpr maishuji::Quad make_right_backdrop() noexcept {
    return make_quad(328.0f, 96.0f, 616.0f, 392.0f, 0.10f,
                     {45, 20, 20, 255});
}

maishuji::Status run_frame(
    maishuji::Pvr &pvr,
    const std::array<maishuji::Quad, workload_quads> &friendly,
    const std::array<maishuji::Quad, workload_quads> &hostile) noexcept {
    maishuji::Frame frame;
    maishuji::Status status = pvr.begin_frame(frame);
    if(maishuji::failed(status))
        return status;

    maishuji::RenderList list;
    status = frame.begin_list(list, maishuji::List::Opaque);
    if(maishuji::failed(status))
        return status;

    const maishuji::Quad static_geometry[] = {
        make_background(), make_left_backdrop(), make_right_backdrop(),
    };
    for(const maishuji::Quad &quad : static_geometry) {
        status = list.submit(quad);
        if(maishuji::failed(status))
            return status;
    }
    for(const maishuji::Quad &quad : friendly) {
        status = list.submit(quad);
        if(maishuji::failed(status))
            return status;
    }
    for(const maishuji::Quad &quad : hostile) {
        status = list.submit(quad);
        if(maishuji::failed(status))
            return status;
    }

    status = list.finish();
    if(maishuji::failed(status))
        return status;
    return frame.finish();
}

int fail(maishuji::Pvr &pvr, maishuji::Status status) noexcept {
    (void)pvr.shutdown();
    dbglog(DBG_ERROR, "maishuji: PVR tile workload failed: %s\n",
           maishuji::status_name(status));
    return 1;
}

} // namespace

int main() {
    const auto friendly = make_tile_friendly_workload();
    const auto hostile = make_tile_hostile_workload();
    const TileStats friendly_stats = estimate(friendly);
    const TileStats hostile_stats = estimate(hostile);

    maishuji::Pvr pvr;
    maishuji::Status status = pvr.initialize();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: PVR initialization failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }

    for(int frame = 0; frame < warmup_frames + capture_hold_frames; ++frame) {
        status = run_frame(pvr, friendly, hostile);
        if(maishuji::failed(status))
            return fail(pvr, status);
        if(frame == warmup_frames - 1) {
            dbglog(DBG_NOTICE,
                   "maishuji: tile workload passed (friendly refs=%u; hostile refs=%u; friendly max=%u; hostile max=%u; estimated 32x32 tiles)\n",
                   friendly_stats.tile_references, hostile_stats.tile_references,
                   friendly_stats.max_tile_layers, hostile_stats.max_tile_layers);
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
