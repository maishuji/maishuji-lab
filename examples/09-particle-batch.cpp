#include "maishuji/pvr.hpp"

#include <array>
#include <cstdint>
#include <span>

#include <kos.h>

namespace {

constexpr std::uint16_t texture_width = 16;
constexpr std::uint16_t texture_height = 16;
constexpr int particle_count = 24;
constexpr int frame_count = 600;
// Keep the rendered batch alive long enough for the emulator checker.
constexpr int capture_hold_frames = 900;

struct Particle {
    int column = 0;
    int origin_x = 0;
    int origin_y = 0;
    int speed = 0;
    int drift = 0;
    int size = 18;
    maishuji::Color color{};
};

constexpr std::array<Particle, particle_count> particles{{
    {0, 4, 8, 1, 1, 18, {255, 48, 48, 255}},
    {0, 38, 52, 2, 1, 18, {255, 48, 48, 255}},
    {0, 72, 96, 1, 2, 18, {255, 48, 48, 255}},
    {0, 106, 140, 2, 1, 18, {255, 48, 48, 255}},
    {0, 20, 188, 1, 2, 18, {255, 48, 48, 255}},
    {0, 54, 232, 2, 1, 18, {255, 48, 48, 255}},
    {0, 88, 276, 1, 2, 18, {255, 48, 48, 255}},
    {0, 118, 320, 2, 1, 18, {255, 48, 48, 255}},
    {1, 12, 28, 2, 1, 18, {48, 255, 96, 255}},
    {1, 46, 72, 1, 2, 18, {48, 255, 96, 255}},
    {1, 80, 116, 2, 1, 18, {48, 255, 96, 255}},
    {1, 114, 160, 1, 2, 18, {48, 255, 96, 255}},
    {1, 28, 204, 2, 1, 18, {48, 255, 96, 255}},
    {1, 62, 248, 1, 2, 18, {48, 255, 96, 255}},
    {1, 96, 292, 2, 1, 18, {48, 255, 96, 255}},
    {1, 124, 336, 1, 2, 18, {48, 255, 96, 255}},
    {2, 4, 44, 1, 2, 18, {64, 128, 255, 255}},
    {2, 38, 88, 2, 1, 18, {64, 128, 255, 255}},
    {2, 72, 132, 1, 2, 18, {64, 128, 255, 255}},
    {2, 106, 176, 2, 1, 18, {64, 128, 255, 255}},
    {2, 20, 220, 1, 2, 18, {64, 128, 255, 255}},
    {2, 54, 264, 2, 1, 18, {64, 128, 255, 255}},
    {2, 88, 308, 1, 2, 18, {64, 128, 255, 255}},
    {2, 118, 352, 2, 1, 18, {64, 128, 255, 255}},
}};

maishuji::TexturedQuad make_particle_quad(const Particle &particle,
                                           int frame_index) noexcept {
    const int left = 40 + particle.column * 200 +
                     ((particle.origin_x + frame_index * particle.speed) % 120);
    const int top = 80 +
                    ((particle.origin_y + frame_index * particle.drift) % 300);
    const float x = static_cast<float>(left);
    const float y = static_cast<float>(top);
    const float size = static_cast<float>(particle.size);

    return {
        {x, y, 1.0f, 0.0f, 0.0f, particle.color},
        {x, y + size, 1.0f, 0.0f, 1.0f, particle.color},
        {x + size, y, 1.0f, 1.0f, 0.0f, particle.color},
        {x + size, y + size, 1.0f, 1.0f, 1.0f, particle.color},
    };
}

maishuji::Status run_frame(maishuji::Pvr &pvr,
                           const maishuji::Texture &texture,
                           int frame_index) noexcept {
    maishuji::Frame frame;
    maishuji::Status status = pvr.begin_frame(frame);
    if(maishuji::failed(status))
        return status;

    maishuji::RenderList list;
    status = frame.begin_list(list, maishuji::List::PunchThrough);
    if(maishuji::failed(status))
        return status;

    for(const Particle &particle : particles) {
        const maishuji::TexturedQuad quad =
            make_particle_quad(particle, frame_index);
        status = list.submit(texture, quad);
        if(maishuji::failed(status))
            return status;
    }

    status = list.finish();
    if(maishuji::failed(status))
        return status;

    return frame.finish();
}

} // namespace

int main() {
    alignas(32) std::array<std::uint16_t,
                           texture_width * texture_height> pixels{};
    for(std::uint16_t y = 0; y < texture_height; ++y) {
        for(std::uint16_t x = 0; x < texture_width; ++x) {
            const int dx = static_cast<int>(x) - 7;
            const int dy = static_cast<int>(y) - 7;
            const std::uint16_t alpha = dx * dx + dy * dy <= 49 ? 15 : 0;
            pixels[y * texture_width + x] =
                static_cast<std::uint16_t>((alpha << 12) | 0x0fff);
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
        dbglog(DBG_ERROR, "maishuji: particle texture allocation failed: %s\n",
               maishuji::status_name(status));
        (void)pvr.shutdown();
        return 1;
    }

    status = texture.upload(
        std::span<const std::uint16_t>{pixels.data(), pixels.size()});
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: particle texture upload failed: %s\n",
               maishuji::status_name(status));
        (void)texture.release();
        (void)pvr.shutdown();
        return 1;
    }

    for(int frame_index = 0; frame_index < frame_count; ++frame_index) {
        status = run_frame(pvr, texture, frame_index);
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR,
                   "maishuji: particle batch frame %d failed: %s\n",
                   frame_index, maishuji::status_name(status));
            (void)texture.release();
            (void)pvr.shutdown();
            return 1;
        }
    }

    dbglog(DBG_NOTICE,
           "maishuji: particle batch passed (%d particles; %d frames)\n",
           particle_count, frame_count);
    for(int frame = 0; frame < capture_hold_frames; ++frame) {
        status = run_frame(pvr, texture, 0);
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR,
                   "maishuji: particle capture hold frame %d failed: %s\n",
                   frame, maishuji::status_name(status));
            (void)texture.release();
            (void)pvr.shutdown();
            return 1;
        }
    }

    status = texture.release();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: particle texture release failed: %s\n",
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
