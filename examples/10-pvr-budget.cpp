#include "maishuji/pvr.hpp"

#include <array>
#include <cstdint>
#include <span>

#include <kos.h>

namespace {

constexpr int frames = 600;
// Keep the final frame visible long enough for the Flycast checker.
constexpr int capture_hold_frames = 900;
constexpr std::uint16_t texture_width = 16;
constexpr std::uint16_t texture_height = 16;

struct PacketBudget {
    int polygon_headers;
    int vertex_packets;
    int primitive_calls;
    int packet_bytes;
};

// Every header and vertex packet emitted by the current KOS path is 32 bytes.
constexpr PacketBudget budget{6, 21, 27, (6 + 21) * 32};
static_assert(budget.polygon_headers == 6);
static_assert(budget.vertex_packets == 3 * 3 + 2 * 4 + 1 * 4);
static_assert(budget.primitive_calls ==
              budget.polygon_headers + budget.vertex_packets);
static_assert(budget.packet_bytes == budget.primitive_calls * 32);

maishuji::Triangle make_triangle(float left, float top,
                                 const maishuji::Color &top_color,
                                 const maishuji::Color &left_color,
                                 const maishuji::Color &right_color) noexcept {
    return {
        {left + 78.0f, top, 1.0f, top_color},
        {left, top + 130.0f, 1.0f, left_color},
        {left + 156.0f, top + 130.0f, 1.0f, right_color},
    };
}

maishuji::Quad make_quad(float left, float top,
                         const maishuji::Color &top_left,
                         const maishuji::Color &bottom_left,
                         const maishuji::Color &top_right,
                         const maishuji::Color &bottom_right) noexcept {
    return {
        {left, top, 1.0f, top_left},
        {left, top + 110.0f, 1.0f, bottom_left},
        {left + 156.0f, top, 1.0f, top_right},
        {left + 156.0f, top + 110.0f, 1.0f, bottom_right},
    };
}

maishuji::TexturedQuad make_textured_quad() noexcept {
    return {
        {242.0f, 310.0f, 1.0f, 0.0f, 0.0f, {80, 220, 255, 255}},
        {242.0f, 420.0f, 1.0f, 0.0f, 1.0f, {80, 220, 255, 255}},
        {398.0f, 310.0f, 1.0f, 1.0f, 0.0f, {80, 220, 255, 255}},
        {398.0f, 420.0f, 1.0f, 1.0f, 1.0f, {80, 220, 255, 255}},
    };
}

maishuji::Status run_frame(maishuji::Pvr &pvr,
                           const maishuji::Texture &texture,
                           const maishuji::Triangle &opaque_triangle,
                           const maishuji::Quad &opaque_quad,
                           const maishuji::Triangle &punch_triangle,
                           const maishuji::Triangle &translucent_triangle,
                           const maishuji::Quad &translucent_quad) noexcept {
    maishuji::Frame frame;
    maishuji::Status status = pvr.begin_frame(frame);
    if(maishuji::failed(status))
        return status;

    maishuji::RenderList opaque;
    status = frame.begin_list(opaque, maishuji::List::Opaque);
    if(maishuji::failed(status))
        return status;
    status = opaque.submit(opaque_triangle);
    if(maishuji::failed(status))
        return status;
    status = opaque.submit(opaque_quad);
    if(maishuji::failed(status))
        return status;
    status = opaque.finish();
    if(maishuji::failed(status))
        return status;

    maishuji::RenderList punch_through;
    status = frame.begin_list(punch_through, maishuji::List::PunchThrough);
    if(maishuji::failed(status))
        return status;
    status = punch_through.submit(punch_triangle);
    if(maishuji::failed(status))
        return status;
    status = punch_through.submit(texture, make_textured_quad());
    if(maishuji::failed(status))
        return status;
    status = punch_through.finish();
    if(maishuji::failed(status))
        return status;

    maishuji::RenderList translucent;
    status = frame.begin_list(translucent, maishuji::List::Translucent);
    if(maishuji::failed(status))
        return status;
    status = translucent.submit(translucent_triangle);
    if(maishuji::failed(status))
        return status;
    status = translucent.submit(translucent_quad);
    if(maishuji::failed(status))
        return status;
    status = translucent.finish();
    if(maishuji::failed(status))
        return status;

    return frame.finish();
}

} // namespace

int main() {
    const maishuji::Triangle opaque_triangle = make_triangle(
        32.0f, 130.0f, {255, 64, 64, 255}, {64, 255, 96, 255},
        {64, 128, 255, 255});
    const maishuji::Quad opaque_quad = make_quad(
        32.0f, 310.0f, {255, 255, 96, 255}, {255, 96, 64, 255},
        {96, 192, 255, 255}, {192, 64, 255, 255});
    const maishuji::Triangle punch_triangle = make_triangle(
        242.0f, 130.0f, {64, 255, 255, 255}, {64, 128, 255, 255},
        {192, 96, 255, 255});
    const maishuji::Triangle translucent_triangle = make_triangle(
        452.0f, 130.0f, {255, 96, 96, 160}, {96, 255, 128, 160},
        {96, 128, 255, 160});
    const maishuji::Quad translucent_quad = make_quad(
        452.0f, 310.0f, {255, 255, 128, 144}, {255, 128, 96, 144},
        {128, 192, 255, 144}, {224, 96, 255, 144});

    alignas(32) std::array<std::uint16_t,
                           texture_width * texture_height> pixels{};
    for(std::uint16_t y = 0; y < texture_height; ++y) {
        for(std::uint16_t x = 0; x < texture_width; ++x) {
            const bool visible = ((x / 2) + (y / 2)) % 2 == 0;
            pixels[y * texture_width + x] = visible ? 0x0fff : 0x0000;
        }
    }

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
        dbglog(DBG_ERROR, "maishuji: budget texture allocation failed: %s\n",
               maishuji::status_name(status));
        (void)pvr.shutdown();
        return 1;
    }

    status = texture.upload(
        std::span<const std::uint16_t>{pixels.data(), pixels.size()});
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: budget texture upload failed: %s\n",
               maishuji::status_name(status));
        (void)texture.release();
        (void)pvr.shutdown();
        return 1;
    }

    for(int frame = 0; frame < frames; ++frame) {
        status = run_frame(pvr, texture, opaque_triangle, opaque_quad,
                           punch_triangle, translucent_triangle,
                           translucent_quad);
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR, "maishuji: budget frame %d failed: %s\n",
                   frame, maishuji::status_name(status));
            (void)texture.release();
            (void)pvr.shutdown();
            return 1;
        }
    }

    dbglog(DBG_NOTICE,
           "maishuji: PVR budget passed (%d headers; %d vertices; "
           "%d primitive calls; %d packet bytes)\n",
           budget.polygon_headers, budget.vertex_packets,
           budget.primitive_calls, budget.packet_bytes);

    for(int frame = 0; frame < capture_hold_frames; ++frame) {
        status = run_frame(pvr, texture, opaque_triangle, opaque_quad,
                           punch_triangle, translucent_triangle,
                           translucent_quad);
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR,
                   "maishuji: budget capture hold frame %d failed: %s\n",
                   frame, maishuji::status_name(status));
            (void)texture.release();
            (void)pvr.shutdown();
            return 1;
        }
    }

    status = texture.release();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: budget texture release failed: %s\n",
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

    return 0;
}
