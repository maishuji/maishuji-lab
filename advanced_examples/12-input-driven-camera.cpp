#include "camera_controls.hpp"

#include "maishuji/mesh.hpp"
#include "maishuji/pvr.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <span>

#include <dc/biosfont.h>
#include <kos.h>
#include <kos/timer.h>

#ifndef MAISHUJI_INPUT_SCRIPTED
#define MAISHUJI_INPUT_SCRIPTED 0
#endif

namespace {
constexpr bool scripted = MAISHUJI_INPUT_SCRIPTED == 1;
constexpr int fixture_frames = 3600;
constexpr std::uint16_t label_width = 512;
constexpr std::uint16_t label_height = 128;

maishuji::ControllerSnapshot fixture(int frame) noexcept {
    maishuji::ControllerSnapshot state{};
    if(frame < 60) return state;
    state.availability = maishuji::InputAvailability::Valid;
    state.capabilities = {maishuji::button_mask(maishuji::Button::Up),
                          true, true, false, false};
    if(frame < 240) state.buttons = maishuji::button_mask(maishuji::Button::Up);
    if(frame >= 240 && frame < 330) state.stick_x = 127;
    return state;
}

maishuji::Camera camera_for(const maishuji::CameraPose &pose) noexcept {
    const float sine = std::sin(pose.yaw);
    const float cosine = std::cos(pose.yaw);
    return {{pose.x, 2.4f, pose.z + 1.6f},
            {pose.x + sine * 5, 0.5f, pose.z + 1.6f - cosine * 5},
            {0, 1, 0}, 1.04719755f, 4.0f / 3.0f, 0.1f, 80.0f};
}

maishuji::Status draw_frame(maishuji::Pvr &pvr, maishuji::Texture &label,
                            const maishuji::Mesh &floor, const maishuji::Mesh &landmark,
                            const maishuji::CameraPose &pose,
                            const maishuji::ControllerSnapshot &sample) noexcept {
    alignas(32) static std::array<std::uint16_t, label_width * label_height> pixels{};
    pixels.fill(0);
    char lines[256]{};
    ::snprintf(lines, sizeof(lines),
               "12 INPUT CAMERA / %s\nPORT A: %s\nPOS X:%+.2f Z:%+.2f  YAW:%+.2f\nDPAD WALK / STICK X TURN Y WALK\n%s",
               scripted ? "SCRIPTED / NOT LIVE" : "LIVE",
               maishuji::input_availability_name(sample.availability),
               static_cast<double>(pose.x), static_cast<double>(pose.z),
               static_cast<double>(pose.yaw),
               scripted ? "FIXTURE / NO MAPLE INPUT" : "START: EXIT / CACHED INPUT");
    bfont_set_encoding(BFONT_CODE_ISO8859_1);
    bfont_draw_str_ex(pixels.data(), label_width, 0xffff, 0, 16, false, lines);
    auto status = pvr.wait_render_done();
    if(maishuji::failed(status)) return status;
    status = label.upload(std::span<const std::uint16_t>{pixels});
    if(maishuji::failed(status)) return status;

    maishuji::Frame frame;
    status = pvr.begin_frame(frame);
    if(maishuji::failed(status)) return status;
    maishuji::RenderList objects;
    status = frame.begin_list(objects, maishuji::List::Opaque);
    if(maishuji::failed(status)) return status;
    const auto camera = camera_for(pose);
    status = objects.submit(floor, camera, {}, {640, 480});
    if(maishuji::failed(status)) return status;
    for(const maishuji::Vec3 position : std::array<maishuji::Vec3, 4>{{
            {-3, 0, -4}, {3, 0, -4}, {-3, 0, -10}, {3, 0, -10}}}) {
        status = objects.submit(landmark, camera,
                                {position, {}, {1, 1, 1}}, {640, 480});
        if(maishuji::failed(status)) return status;
    }
    status = objects.finish();
    if(maishuji::failed(status)) return status;
    maishuji::RenderList text;
    status = frame.begin_list(text, maishuji::List::PunchThrough);
    if(maishuji::failed(status)) return status;
    status = text.submit(label, maishuji::TexturedQuad{
        {18, 14, 1, 0, 0}, {18, 142, 1, 0, 1},
        {530, 14, 1, 1, 0}, {530, 142, 1, 1, 1},
    });
    if(maishuji::failed(status)) return status;
    status = text.finish();
    if(maishuji::failed(status)) return status;
    return frame.finish();
}
} // namespace

int main() {
    // Fixed storage keeps the world layout inspectable without a scene graph.
    std::array<maishuji::MeshVertex, 96> floor_vertices{};
    std::array<std::uint16_t, 144> floor_indices{};
    for(int row = 0; row < 6; ++row) {
        for(int column = 0; column < 4; ++column) {
            const int cell = row * 4 + column;
            const float x = -8.0f + column * 4.0f;
            const float z = -16.0f + row * 4.0f;
            const maishuji::Color color = (row + column) % 2 == 0 ?
                maishuji::Color{48, 92, 104, 255} : maishuji::Color{66, 115, 126, 255};
            const int v = cell * 4;
            floor_vertices[v] = {{x, 0, z}, color};
            floor_vertices[v + 1] = {{x + 4, 0, z}, color};
            floor_vertices[v + 2] = {{x + 4, 0, z + 4}, color};
            floor_vertices[v + 3] = {{x, 0, z + 4}, color};
            const int index = cell * 6;
            floor_indices[index] = static_cast<std::uint16_t>(v);
            floor_indices[index + 1] = static_cast<std::uint16_t>(v + 1);
            floor_indices[index + 2] = static_cast<std::uint16_t>(v + 2);
            floor_indices[index + 3] = static_cast<std::uint16_t>(v);
            floor_indices[index + 4] = static_cast<std::uint16_t>(v + 2);
            floor_indices[index + 5] = static_cast<std::uint16_t>(v + 3);
        }
    }
    const maishuji::Mesh floor{floor_vertices, floor_indices};
    constexpr maishuji::Color gold{255, 196, 94, 255};
    constexpr maishuji::Color teal{101, 230, 159, 255};
    const std::array<maishuji::MeshVertex, 8> post_vertices{{
        {{-0.45f, 0, -0.45f}, gold}, {{0.45f, 0, -0.45f}, gold},
        {{0.45f, 1.5f, -0.45f}, teal}, {{-0.45f, 1.5f, -0.45f}, teal},
        {{-0.45f, 0, 0.45f}, gold}, {{0.45f, 0, 0.45f}, gold},
        {{0.45f, 1.5f, 0.45f}, teal}, {{-0.45f, 1.5f, 0.45f}, teal},
    }};
    const std::array<std::uint16_t, 36> post_indices{{
        0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6,
        0, 4, 5, 0, 5, 1, 3, 2, 6, 3, 6, 7,
        0, 3, 7, 0, 7, 4, 1, 5, 6, 1, 6, 2,
    }};
    const maishuji::Mesh posts{post_vertices, post_indices};

    maishuji::Pvr pvr;
    auto status = pvr.initialize();
    if(maishuji::failed(status)) return 1;
    maishuji::Texture label;
    status = label.allocate(pvr, label_width, label_height);
    if(maishuji::failed(status)) { (void)pvr.shutdown(); return 1; }

    maishuji::CameraPose pose{};
    std::uint64_t previous_ms = timer_ms_gettime64();
    int frame = 0;
    while(maishuji::succeeded(status)) {
        const std::uint64_t now_ms = timer_ms_gettime64();
        const float elapsed = scripted ? 1.0f / 60.0f :
            static_cast<float>(now_ms - previous_ms) / 1000.0f;
        previous_ms = now_ms;
        const auto sample = scripted ? fixture(frame) :
            maishuji::sample_controller(maishuji::ControllerPort::A);
        if(!scripted && sample.held(maishuji::Button::Start)) break;
        maishuji::update_camera_pose(pose, sample, elapsed);
        status = draw_frame(pvr, label, floor, posts, pose, sample);
        if(maishuji::failed(status)) break;
        ++frame;
        if(scripted && frame == 360) {
            if(pose.z >= -0.1f || pose.yaw <= 0.1f) {
                status = maishuji::Status::InvalidConfiguration;
                break;
            }
            const auto wait = pvr.wait_render_done();
            if(maishuji::failed(wait)) { status = wait; break; }
            dbglog(DBG_NOTICE, "maishuji: input camera scripted passed\n");
        }
        if(scripted && frame >= fixture_frames) break;
    }
    const auto wait = pvr.wait_render_done();
    if(maishuji::succeeded(status)) status = wait;
    const auto released = label.release();
    if(maishuji::succeeded(status)) status = released;
    const auto shutdown = pvr.shutdown();
    if(maishuji::succeeded(status)) status = shutdown;
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: input camera failed: %s\n", maishuji::status_name(status));
        return 1;
    }
    return 0;
}
