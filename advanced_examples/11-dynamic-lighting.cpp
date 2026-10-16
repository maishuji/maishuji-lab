#include "dynamic_lighting.hpp"
#include "car_2cv_mesh.hpp"
#include "car_2cv_texture.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

#include <dc/biosfont.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <kos.h>

#ifndef MAISHUJI_DYNAMIC_LIGHTING_CAPTURE
#define MAISHUJI_DYNAMIC_LIGHTING_CAPTURE 0
#endif

namespace {

namespace lighting = dynamic_lighting;
constexpr bool capture_mode = MAISHUJI_DYNAMIC_LIGHTING_CAPTURE == 1;
constexpr std::size_t car_index_count = maishuji::car_2cv_mesh::indices.size();
static_assert(car_index_count == 498 * 3, "Expected the authored 498-triangle 2CV");
lighting::FlatCar<car_index_count> car;

constexpr maishuji::Camera camera = capture_mode ? lighting::capture_camera
                                                : lighting::normal_camera;
constexpr maishuji::Color sky_color{27, 40, 59, 255};
constexpr maishuji::PrimitiveConfiguration overlay_configuration{
    maishuji::Culling::None, maishuji::DepthCompare::Always, false, false,
};
constexpr float extent = lighting::ground_half_extent;
constexpr std::array<maishuji::MeshVertex, 4> floor_vertices{{
    {{-extent, 0, -extent}, lighting::ground_color},
    {{-extent, 0, extent}, lighting::ground_color},
    {{extent, 0, -extent}, lighting::ground_color},
    {{extent, 0, extent}, lighting::ground_color},
}};
constexpr std::array<std::uint16_t, 6> floor_indices{0, 1, 2, 2, 1, 3};
constexpr maishuji::Mesh floor_mesh{floor_vertices, floor_indices};

constexpr std::size_t ring_segments = 96;
std::array<maishuji::MeshVertex, (ring_segments + 1) * 2> ring_vertices{};
std::array<std::uint16_t, ring_segments * 6> ring_indices{};
constexpr std::array<maishuji::Vec3, 6> marker_points{{
    {0, 0.18f, 0}, {0, -0.18f, 0}, {0.16f, 0, 0},
    {0, 0, 0.16f}, {-0.16f, 0, 0}, {0, 0, -0.16f},
}};
constexpr std::array<std::uint16_t, 24> marker_faces{
    0, 2, 3, 0, 3, 4, 0, 4, 5, 0, 5, 2,
    1, 3, 2, 1, 4, 3, 1, 5, 4, 1, 2, 5,
};
std::array<maishuji::MeshVertex, 24> marker_vertices{};
std::array<std::uint16_t, 24> marker_indices{};

void initialize_guides() noexcept {
    for(std::size_t i = 0; i <= ring_segments; ++i) {
        const float angle = 2.0f * lighting::pi * static_cast<float>(i) /
                            static_cast<float>(ring_segments);
        for(std::size_t edge = 0; edge < 2; ++edge) {
            const float radius = lighting::orbit_radius +
                                 (edge == 0 ? -0.018f : 0.018f);
            ring_vertices[i * 2 + edge] = {
                {radius * std::cos(angle), 0.001f, radius * std::sin(angle)},
                {173, 146, 99, 255},
            };
        }
        if(i == ring_segments) continue;
        const auto a = static_cast<std::uint16_t>(i * 2);
        ring_indices[i * 6] = a;
        ring_indices[i * 6 + 1] = a + 1;
        ring_indices[i * 6 + 2] = a + 2;
        ring_indices[i * 6 + 3] = a + 2;
        ring_indices[i * 6 + 4] = a + 1;
        ring_indices[i * 6 + 5] = a + 3;
    }
    for(std::size_t i = 0; i < marker_indices.size(); ++i) {
        marker_indices[i] = static_cast<std::uint16_t>(i);
        marker_vertices[i].color = i < 12 ? maishuji::Color{255, 236, 161, 255}
                                         : maishuji::Color{231, 165, 64, 255};
    }
}

void update_marker(maishuji::Vec3 light) noexcept {
    for(std::size_t i = 0; i < marker_vertices.size(); ++i)
        marker_vertices[i].position = light + marker_points[marker_faces[i]];
}

constexpr maishuji::Quad rectangle(float left, float top, float right,
                                    float bottom, maishuji::Color color,
                                    float depth = 0.0f) noexcept {
    return {{left, top, depth, color}, {left, bottom, depth, color},
            {right, top, depth, color}, {right, bottom, depth, color}};
}

constexpr std::uint16_t label_width = 512;
constexpr std::uint16_t label_height = 32;
enum Label : std::size_t { Title, Footer, CaptureLeft, CaptureRight, LabelCount };

maishuji::Status upload_label(maishuji::Pvr &pvr, maishuji::Texture &texture,
                              const char *message) noexcept {
    alignas(32) static std::array<std::uint16_t, label_width * label_height> pixels{};
    pixels.fill(0);
    bfont_set_encoding(BFONT_CODE_ISO8859_1);
    bfont_draw_str_ex(pixels.data(), label_width, 0xffff, 0, 16, false, message);
    auto status = texture.allocate(pvr, label_width, label_height);
    if(maishuji::failed(status)) return status;
    return texture.upload(std::span<const std::uint16_t>{pixels});
}

maishuji::Status draw_label(maishuji::RenderList &list,
                            const maishuji::Texture &texture,
                            float left, float top, float scale = 1.0f) noexcept {
    const float right = left + label_width * scale;
    const float bottom = top + label_height * scale;
    return list.submit(texture, maishuji::TexturedQuad{
        {left, top, 1, 0, 0}, {left, bottom, 1, 0, 1},
        {right, top, 1, 1, 0}, {right, bottom, 1, 1, 1},
    }, overlay_configuration);
}

maishuji::Status draw_scene(maishuji::RenderList &list,
                            const maishuji::Texture &texture,
                            maishuji::Vec3 light, bool shadows,
                            const maishuji::Transform &transform = {}) noexcept {
    // Lighting and projection are model-local; a rigid yaw/ground translation
    // preserves both. Submit all three meshes with the same model transform.
    if(!car.update(light)) return maishuji::Status::MeshInvalidData;
    update_marker(light);
    const maishuji::Mesh shadow{car.shadow, car.indices};
    const maishuji::Mesh marker{marker_vertices, marker_indices};
    const maishuji::TexturedMesh body{car.lit, car.indices};
    if(shadows) {
        // Opaque, constant-color coverage: overlapping projected triangles
        // replace the same color, never accumulate translucent darkness.
        const auto status = list.submit(shadow, camera, transform, {640, 480},
                                         lighting::shadow_configuration);
        if(maishuji::failed(status)) return status;
    }
    const auto status = list.submit(texture, body, camera, transform, {640, 480});
    if(maishuji::failed(status)) return status;
    return list.submit(marker, camera, transform, {640, 480});
}

maishuji::Status run_frame(maishuji::Pvr &pvr, const maishuji::Texture &texture,
                           const std::array<maishuji::Texture, LabelCount> &labels,
                           float angle, bool shadows) noexcept {
    maishuji::Frame frame;
    auto status = pvr.begin_frame(frame);
    if(maishuji::failed(status)) return status;
    maishuji::RenderList opaque;
    status = frame.begin_list(opaque, lighting::shadow_list);
    if(maishuji::failed(status)) return status;
    // The mesh depth mapping is reversed; z=0 must not occlude far geometry.
    constexpr maishuji::PrimitiveConfiguration background_configuration{
        maishuji::Culling::None, maishuji::DepthCompare::Always, false, false,
    };
    status = opaque.submit(rectangle(0, 0, 640, 480, sky_color),
                            background_configuration);
    if(maishuji::failed(status)) return status;
    status = opaque.submit(floor_mesh, camera, {}, {640, 480});
    if(maishuji::failed(status)) return status;
    if constexpr(capture_mode) {
        for(std::size_t instance = 0; instance < lighting::capture_transforms.size(); ++instance) {
            status = draw_scene(opaque, texture, lighting::capture_lights[instance],
                                 true, lighting::capture_transforms[instance]);
            if(maishuji::failed(status)) return status;
        }
    } else {
        const maishuji::Mesh ring{ring_vertices, ring_indices};
        status = opaque.submit(ring, camera, {}, {640, 480});
        if(maishuji::failed(status)) return status;
        status = draw_scene(opaque, texture, lighting::orbit_light(angle), shadows);
        if(maishuji::failed(status)) return status;
    }
    status = opaque.submit(rectangle(0, 0, 640, capture_mode ? 64 : 40,
                                      sky_color, 1), overlay_configuration);
    if(maishuji::failed(status)) return status;
    status = opaque.submit(rectangle(0, 440, 640, 480, sky_color, 1),
                            overlay_configuration);
    if(maishuji::failed(status)) return status;
    status = opaque.finish();
    if(maishuji::failed(status)) return status;

    maishuji::RenderList text;
    status = frame.begin_list(text, maishuji::List::PunchThrough);
    if(maishuji::failed(status)) return status;
    status = draw_label(text, labels[Title], 16, 8);
    if(maishuji::failed(status)) return status;
    status = draw_label(text, labels[Footer], 16, 448);
    if(maishuji::failed(status)) return status;
    if constexpr(capture_mode) {
        status = draw_label(text, labels[CaptureLeft], 16, 42, 0.75f);
        if(maishuji::failed(status)) return status;
        status = draw_label(text, labels[CaptureRight], 336, 42, 0.75f);
        if(maishuji::failed(status)) return status;
    }
    status = text.finish();
    if(maishuji::failed(status)) return status;
    return frame.finish();
}

maishuji::Status cleanup(maishuji::Pvr &pvr, maishuji::Texture &texture,
                         std::array<maishuji::Texture, LabelCount> &labels) noexcept {
    // Wait before releasing textures still referenced by the previous scene.
    auto first = pvr.wait_render_done();
    const auto remember = [&first](maishuji::Status status) {
        if(maishuji::succeeded(first) && maishuji::failed(status)) first = status;
    };
    for(auto &label : labels)
        if(label.allocated()) remember(label.release());
    if(texture.allocated()) remember(texture.release());
    remember(pvr.shutdown());
    return first;
}

} // namespace

int main() {
    if(!car.initialize(maishuji::car_2cv_mesh::vertices,
                        maishuji::car_2cv_mesh::indices)) {
        dbglog(DBG_ERROR, "maishuji: dynamic lighting invalid car geometry\n");
        return 1;
    }
    initialize_guides();
    maishuji::Pvr pvr;
    maishuji::Configuration configuration{};
    configuration.enable_translucent = false;
    auto status = pvr.initialize(configuration);
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: dynamic lighting initialize failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }
    maishuji::Texture texture;
    std::array<maishuji::Texture, LabelCount> labels;
    alignas(32) static auto texture_pixels = maishuji::car_2cv_texture::pixels;
    status = texture.allocate(pvr, maishuji::car_2cv_texture::width,
                               maishuji::car_2cv_texture::height);
    if(maishuji::succeeded(status))
        status = texture.upload(std::span<const std::uint16_t>{texture_pixels});
    constexpr const char *messages[] = {
        "11 / DYNAMIC LIGHTING / BLUE + CREAM 2CV",
        capture_mode ? "OPPOSITE LIGHTS / OPAQUE PLANAR SHADOWS"
                     : "A PAUSE  B REVERSE  X SHADOW  START EXIT",
        "0 RAD / LIGHT +X", "PI RAD / LIGHT -X",
    };
    for(std::size_t i = 0; i < LabelCount && maishuji::succeeded(status); ++i)
        status = upload_label(pvr, labels[i], messages[i]);

    bool paused = false;
    bool shadows = true;
    int direction = 1;
    int phase = 0;
    unsigned previous_buttons = 0;
    unsigned successful_frames = 0;
    while(maishuji::succeeded(status)) {
        unsigned buttons = 0;
        auto *device = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        if(device != nullptr) {
            const auto *state =
                static_cast<const cont_state_t *>(maple_dev_status(device));
            if(state != nullptr) buttons = state->buttons;
        }
        const unsigned pressed = buttons & ~previous_buttons;
        previous_buttons = buttons;
        if((pressed & CONT_START) != 0) break;
        if constexpr(!capture_mode) {
            if((pressed & CONT_A) != 0) paused = !paused;
            if((pressed & CONT_B) != 0) direction = -direction;
            if((pressed & CONT_X) != 0) shadows = !shadows;
        }
        const float angle = 2.0f * lighting::pi * static_cast<float>(phase) / 720.0f;
        status = run_frame(pvr, texture, labels, angle, shadows);
        if(maishuji::failed(status)) break;
        ++successful_frames;
        if(successful_frames == 120) {
            status = pvr.wait_render_done();
            if(maishuji::failed(status)) break;
            if constexpr(capture_mode) {
                dbglog(DBG_NOTICE,
                    "maishuji: dynamic lighting capture ready (opposite lights; planar shadows)\n");
            } else {
                dbglog(DBG_NOTICE,
                    "maishuji: dynamic lighting ready (498 triangles; flat Lambert; planar shadows)\n");
            }
        }
        // Deterministic fallback: no timer API was verified in this repository.
        // One revolution per 720 successful frames (12 seconds only at 60 Hz).
        if(!capture_mode && !paused) phase = (phase + direction + 720) % 720;
    }
    const auto cleanup_status = cleanup(pvr, texture, labels);
    if(maishuji::succeeded(status)) status = cleanup_status;
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: dynamic lighting failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }
    return 0;
}
