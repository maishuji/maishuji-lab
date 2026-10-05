#include "maishuji/frustum.hpp"
#include "maishuji/mesh.hpp"
#include "maishuji/pvr.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <span>

#include <dc/biosfont.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <kos.h>

namespace {

constexpr int warmup_frames = 180;
constexpr int capture_hold_frames = 900;
constexpr maishuji::BoundingSphere bound{{}, 0.75f};
constexpr maishuji::Camera camera{
    {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f},
    1.04719755f, 4.0f / 3.0f, 0.1f, 20.0f,
};
constexpr std::array<maishuji::Transform, 4> objects{{
    {{-1.2f, 0.9f, -4.0f}, {}, {1.0f, 1.0f, 1.0f}}, // green: inside
    {{3.0f, 0.9f, -4.0f}, {}, {1.0f, 1.0f, 1.0f}},  // yellow: crosses right plane
    {{5.0f, 0.9f, -4.0f}, {}, {1.0f, 1.0f, 1.0f}},  // red: outside
    {{0.0f, 0.9f, 2.0f}, {}, {1.0f, 1.0f, 1.0f}},   // red: behind camera
}};
constexpr std::array<maishuji::MeshVertex, 4> vertices{{
    {{-0.5f, -0.5f, 0.0f}, {90, 235, 140, 255}},
    {{0.5f, -0.5f, 0.0f}, {245, 215, 70, 255}},
    {{0.0f, 0.5f, 0.0f}, {100, 190, 245, 255}},
    {{0.0f, 0.0f, 0.5f}, {255, 180, 100, 255}},
}};
constexpr std::array<std::uint16_t, 12> indices{{
    0, 1, 2, 0, 3, 1, 1, 3, 2, 2, 3, 0,
}};
constexpr std::uint16_t text_width = 512;
constexpr std::uint16_t text_height = 128;

struct Counts {
    int objects = 0;
    int input_triangles = 0;
};

constexpr maishuji::Quad panel(float left, float top, float right,
                                float bottom, maishuji::Color color,
                                float depth = 0.16f) noexcept {
    return {{left, top, depth, color}, {left, bottom, depth, color},
            {right, top, depth, color}, {right, bottom, depth, color}};
}

maishuji::Status line(maishuji::RenderList &list, float x0, float y0,
                      float x1, float y1, float width,
                      maishuji::Color color) noexcept {
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float scale = width / (2.0f * std::sqrt(dx * dx + dy * dy));
    const float ox = -dy * scale;
    const float oy = dx * scale;
    return list.submit(maishuji::Quad{
        {x0 + ox, y0 + oy, 0.16f, color},
        {x0 - ox, y0 - oy, 0.16f, color},
        {x1 + ox, y1 + oy, 0.16f, color},
        {x1 - ox, y1 - oy, 0.16f, color},
    });
}

maishuji::Status draw_map(maishuji::RenderList &list) noexcept {
    maishuji::Status status = list.submit(panel(16, 248, 624, 472,
                                                {14, 24, 43, 255}, 0.08f));
    if(maishuji::failed(status)) return status;
    // Top-down map: camera at (320, 330); 35 pixels per world X unit and
    // 23 pixels per world depth unit. The lines show the two side planes.
    status = line(list, 320, 330, 166, 462, 2, {70, 120, 190, 255});
    if(maishuji::failed(status)) return status;
    status = line(list, 320, 330, 474, 462, 2, {70, 120, 190, 255});
    if(maishuji::failed(status)) return status;
    status = list.submit(panel(314, 324, 326, 336, {220, 230, 245, 255}));
    if(maishuji::failed(status)) return status;
    constexpr maishuji::Color colors[] = {
        {70, 220, 110, 255}, {245, 210, 65, 255},
        {225, 75, 75, 255}, {225, 75, 75, 255},
    };
    for(std::size_t index = 0; index < objects.size(); ++index) {
        const auto &position = objects[index].position;
        const float x = 320.0f + position.x * 35.0f;
        const float y = 330.0f - position.z * 23.0f;
        // At this map scale a 0.75-world-unit sphere spans 26.25 pixels
        // from its center. A square outline makes that bound easy to read.
        constexpr float radius = bound.radius * 35.0f;
        const maishuji::Color outline{85, 105, 130, 255};
        status = line(list, x - radius, y - radius,
                      x + radius, y - radius, 1, outline);
        if(maishuji::failed(status)) return status;
        status = line(list, x + radius, y - radius,
                      x + radius, y + radius, 1, outline);
        if(maishuji::failed(status)) return status;
        status = line(list, x + radius, y + radius,
                      x - radius, y + radius, 1, outline);
        if(maishuji::failed(status)) return status;
        status = line(list, x - radius, y + radius,
                      x - radius, y - radius, 1, outline);
        if(maishuji::failed(status)) return status;
        status = list.submit(panel(x - 12, y - 12, x + 12, y + 12,
                                   colors[index]));
        if(maishuji::failed(status)) return status;
    }
    return maishuji::Status::Success;
}

maishuji::TexturedQuad text_quad() noexcept {
    return {{16, 8, 1, 0, 0}, {16, 136, 1, 0, 1},
            {528, 8, 1, 1, 0}, {528, 136, 1, 1, 1}};
}

maishuji::Status upload_label(maishuji::Pvr &pvr, maishuji::Texture &texture,
                              const char *message) noexcept {
    alignas(32) static std::array<std::uint16_t,
                                  text_width * text_height> pixels{};
    pixels.fill(0);
    bfont_set_encoding(BFONT_CODE_ISO8859_1);
    bfont_draw_str_ex(pixels.data(), text_width, 0xffff, 0x0000, 16, false,
                      message);
    maishuji::Status status = texture.allocate(pvr, text_width, text_height);
    if(maishuji::failed(status)) return status;
    return texture.upload(std::span<const std::uint16_t>{pixels});
}

maishuji::Status run_frame(maishuji::Pvr &pvr, const maishuji::Mesh &mesh,
                           const maishuji::Texture &label, bool culling,
                           Counts &counts) noexcept {
    maishuji::Frame frame;
    maishuji::Status status = pvr.begin_frame(frame);
    if(maishuji::failed(status)) return status;
    maishuji::RenderList opaque;
    status = frame.begin_list(opaque, maishuji::List::Opaque);
    if(maishuji::failed(status)) return status;
    status = opaque.submit(panel(0, 0, 640, 480, {6, 12, 26, 255},
                                 0.02f));
    if(maishuji::failed(status)) return status;
    status = draw_map(opaque);
    if(maishuji::failed(status)) return status;

    counts = {};
    for(const auto &object : objects) {
        const auto visibility = maishuji::classify_sphere(camera, object, bound);
        if(culling && visibility == maishuji::FrustumVisibility::Outside)
            continue; // Saved CPU transform/clip work; no mesh submission.
        ++counts.objects;
        counts.input_triangles += static_cast<int>(indices.size() / 3);
        status = opaque.submit(mesh, camera, object, {640.0f, 480.0f});
        if(maishuji::failed(status)) return status;
    }
    status = opaque.finish();
    if(maishuji::failed(status)) return status;

    maishuji::RenderList text;
    status = frame.begin_list(text, maishuji::List::PunchThrough);
    if(maishuji::failed(status)) return status;
    status = text.submit(label, text_quad());
    if(maishuji::failed(status)) return status;
    status = text.finish();
    if(maishuji::failed(status)) return status;
    return frame.finish();
}

int fail(maishuji::Pvr &pvr, maishuji::Texture &on,
         maishuji::Texture &off, maishuji::Status status) noexcept {
    dbglog(DBG_ERROR, "maishuji: frustum culling failed: %s\n",
           maishuji::status_name(status));
    (void)on.release();
    (void)off.release();
    (void)pvr.shutdown();
    return 1;
}

} // namespace

int main() {
    constexpr maishuji::FrustumVisibility expected[] = {
        maishuji::FrustumVisibility::Inside,
        maishuji::FrustumVisibility::Intersects,
        maishuji::FrustumVisibility::Outside,
        maishuji::FrustumVisibility::Outside,
    };
    for(std::size_t index = 0; index < objects.size(); ++index) {
        if(maishuji::classify_sphere(camera, objects[index], bound) !=
           expected[index]) {
            dbglog(DBG_ERROR, "maishuji: fixed frustum case %u changed\n",
                   static_cast<unsigned>(index));
            return 1;
        }
    }
    const maishuji::Mesh mesh{vertices, indices};
    maishuji::Pvr pvr;
    maishuji::Status status = pvr.initialize();
    if(maishuji::failed(status)) return 1;

    maishuji::Texture on;
    maishuji::Texture off;
    status = upload_label(pvr, on,
        "FRUSTUM CULLING: ON  (A TO TOGGLE)\n"
        "GREEN INSIDE / YELLOW EDGE / RED OUT\n"
        "NO CULL: 4 OBJECTS / 16 INPUT TRIS\n"
        "CULL:    2 OBJECTS /  8 INPUT TRIS\n"
        "MAP BELOW: CAMERA + SIDE PLANES");
    if(maishuji::failed(status)) return fail(pvr, on, off, status);
    status = upload_label(pvr, off,
        "FRUSTUM CULLING: OFF (A TO TOGGLE)\n"
        "GREEN INSIDE / YELLOW EDGE / RED OUT\n"
        "NO CULL: 4 OBJECTS / 16 INPUT TRIS\n"
        "CULL:    2 OBJECTS /  8 INPUT TRIS\n"
        "MAP BELOW: CAMERA + SIDE PLANES");
    if(maishuji::failed(status)) return fail(pvr, on, off, status);

    bool culling = true;
    bool previous_a = false;
    for(int frame_index = 0;
        frame_index < warmup_frames + capture_hold_frames; ++frame_index) {
        bool a = false;
        maple_device_t *device = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        if(device != nullptr) {
            const auto *state =
                static_cast<const cont_state_t *>(maple_dev_status(device));
            a = state != nullptr && (state->buttons & CONT_A) != 0;
        }
        if(a && !previous_a) culling = !culling;
        previous_a = a;

        Counts counts{};
        status = run_frame(pvr, mesh, culling ? on : off, culling, counts);
        if(maishuji::failed(status)) return fail(pvr, on, off, status);
        if(frame_index == warmup_frames - 1) {
            dbglog(DBG_NOTICE,
                   "maishuji: frustum culling passed (mode=%s; submitted=%d objects; input=%d triangles; culled baseline=2 objects/8 triangles)\n",
                   culling ? "on" : "off", counts.objects,
                   counts.input_triangles);
        }
    }
    status = on.release();
    if(maishuji::failed(status)) return fail(pvr, on, off, status);
    status = off.release();
    if(maishuji::failed(status)) return fail(pvr, on, off, status);
    status = pvr.shutdown();
    if(maishuji::failed(status)) return 1;
    return 0;
}
