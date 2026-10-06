#include "maishuji/mesh.hpp"
#include "maishuji/pvr.hpp"

#ifdef MAISHUJI_HUMAN_MODEL
#include "assets/human_model_asset.hpp"
#include "assets/human_model_texture.hpp"
namespace lesson_asset = maishuji::human_model_asset;
namespace lesson_texture = maishuji::human_model_texture;
#else
#include "assets/model_asset.hpp"
#include "assets/model_texture.hpp"
namespace lesson_asset = maishuji::model_asset;
namespace lesson_texture = maishuji::model_texture;
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <kos.h>

namespace {

constexpr std::size_t dcm_header_bytes = 24;
constexpr std::size_t dcm_vertex_bytes = 16;
constexpr std::uint16_t dcm_version = 1;
constexpr std::uint16_t dcm_textured_flag = 1;
constexpr float dcm_position_scale = 256.0f;
constexpr float dcm_uv_scale = 65535.0f;
#ifdef MAISHUJI_HUMAN_MODEL
constexpr std::size_t max_model_vertices = 512;
constexpr std::size_t max_model_indices = 768;
#else
constexpr std::size_t max_model_vertices = 96;
constexpr std::size_t max_model_indices = 192;
#endif
constexpr int animation_frames = 180;
constexpr int capture_hold_frames = 900;
constexpr float screen_width = 640.0f;
constexpr float screen_height = 480.0f;

std::uint16_t read_le16(std::span<const std::uint8_t> bytes,
                        std::size_t offset) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(bytes[offset]) |
        (static_cast<std::uint16_t>(bytes[offset + 1]) << 8));
}

std::uint32_t read_le32(std::span<const std::uint8_t> bytes,
                        std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::int16_t read_le_i16(std::span<const std::uint8_t> bytes,
                         std::size_t offset) noexcept {
    return static_cast<std::int16_t>(read_le16(bytes, offset));
}

bool has_fourcc(std::span<const std::uint8_t> bytes,
                const char (&fourcc)[5]) noexcept {
    for(std::size_t index = 0; index < 4; ++index) {
        if(bytes[index] != static_cast<std::uint8_t>(fourcc[index]))
            return false;
    }
    return true;
}

maishuji::Color unpack_argb(std::uint32_t argb) noexcept {
    return {
        static_cast<std::uint8_t>((argb >> 16) & 0xffu),
        static_cast<std::uint8_t>((argb >> 8) & 0xffu),
        static_cast<std::uint8_t>(argb & 0xffu),
        static_cast<std::uint8_t>((argb >> 24) & 0xffu),
    };
}

struct LoadedModel {
    std::array<maishuji::TexturedMeshVertex, max_model_vertices> vertices{};
    std::array<std::uint16_t, max_model_indices> indices{};
    std::uint16_t vertex_count = 0;
    std::uint16_t index_count = 0;

    bool load(std::span<const std::uint8_t> file) noexcept {
        if(file.size() < dcm_header_bytes || !has_fourcc(file, "DCM1"))
            return false;

        const std::uint16_t version = read_le16(file, 4);
        const std::uint16_t flags = read_le16(file, 6);
        const std::uint16_t parsed_vertex_count = read_le16(file, 8);
        const std::uint16_t parsed_index_count = read_le16(file, 10);
        const std::uint32_t vertex_offset = read_le32(file, 12);
        const std::uint32_t index_offset = read_le32(file, 16);
        const std::uint32_t file_size = read_le32(file, 20);

        if(version != dcm_version || flags != dcm_textured_flag ||
           parsed_vertex_count == 0 || parsed_index_count == 0 ||
           parsed_index_count % 3 != 0 ||
           parsed_vertex_count > max_model_vertices ||
           parsed_index_count > max_model_indices ||
           file_size != file.size() || vertex_offset != dcm_header_bytes)
            return false;

        const std::size_t expected_index_offset =
            dcm_header_bytes +
            static_cast<std::size_t>(parsed_vertex_count) * dcm_vertex_bytes;
        const std::size_t expected_file_size =
            expected_index_offset +
            static_cast<std::size_t>(parsed_index_count) * sizeof(std::uint16_t);
        if(index_offset != expected_index_offset ||
           file_size != expected_file_size)
            return false;

        for(std::uint16_t index = 0; index < parsed_vertex_count; ++index) {
            const std::size_t offset =
                dcm_header_bytes + static_cast<std::size_t>(index) *
                                       dcm_vertex_bytes;
            const std::uint32_t argb = read_le32(file, offset + 10);
            vertices[index] = {
                {
                    static_cast<float>(read_le_i16(file, offset)) /
                        dcm_position_scale,
                    static_cast<float>(read_le_i16(file, offset + 2)) /
                        dcm_position_scale,
                    static_cast<float>(read_le_i16(file, offset + 4)) /
                        dcm_position_scale,
                },
                static_cast<float>(read_le16(file, offset + 6)) / dcm_uv_scale,
                static_cast<float>(read_le16(file, offset + 8)) / dcm_uv_scale,
                unpack_argb(argb),
            };
        }

        for(std::uint16_t index = 0; index < parsed_index_count; ++index) {
            const std::size_t offset =
                expected_index_offset + static_cast<std::size_t>(index) * 2;
            const std::uint16_t vertex_index = read_le16(file, offset);
            if(vertex_index >= parsed_vertex_count)
                return false;
            indices[index] = vertex_index;
        }

        vertex_count = parsed_vertex_count;
        index_count = parsed_index_count;
        return true;
    }

    maishuji::TexturedMesh mesh() const noexcept {
        return {
            std::span<const maishuji::TexturedMeshVertex>{vertices.data(),
                                                          vertex_count},
            std::span<const std::uint16_t>{indices.data(), index_count},
        };
    }
};

maishuji::Transform transform_for_frame(int frame) noexcept {
    const float angle = static_cast<float>(frame) * 0.023f;
    return {
        {0.0f, -0.05f, 0.0f},
        {0.20f, angle, angle * 0.55f},
        {1.0f, 1.0f, 1.0f},
    };
}

constexpr maishuji::Transform capture_transform() noexcept {
#ifdef MAISHUJI_HUMAN_MODEL
    return {
        {0.0f, -0.05f, 0.0f},
        {0.0f, 0.30f, 0.0f},
        {1.0f, 1.0f, 1.0f},
    };
#else
    return {
        {0.0f, -0.05f, 0.0f},
        {0.23f, 0.78f, -0.18f},
        {1.0f, 1.0f, 1.0f},
    };
#endif
}

constexpr maishuji::Camera model_camera() noexcept {
    return {
        {0.0f, 0.35f, 5.8f},
        {0.0f, 0.20f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        0.92f,
        4.0f / 3.0f,
        0.1f,
        100.0f,
    };
}

maishuji::Status run_frame(
    maishuji::Pvr &pvr, const maishuji::Texture &texture,
    const maishuji::TexturedMesh &model,
    const maishuji::Camera &camera,
    const maishuji::Transform &transform) noexcept {
    maishuji::Frame frame;
    maishuji::Status status = pvr.begin_frame(frame);
    if(maishuji::failed(status))
        return status;

    maishuji::RenderList list;
    status = frame.begin_list(list, maishuji::List::Opaque);
    if(maishuji::failed(status))
        return status;
    status = list.submit(texture, model, camera, transform,
                         maishuji::Viewport{screen_width, screen_height});
    if(maishuji::failed(status))
        return status;
    status = list.finish();
    if(maishuji::failed(status))
        return status;

    return frame.finish();
}

} // namespace

int main() {
#ifndef MAISHUJI_HUMAN_MODEL
    static_assert(maishuji::model_asset::bytes.size() == 1336);
#endif
    alignas(32) static std::array<std::uint16_t,
                                  lesson_texture::width * lesson_texture::height>
        texture_pixels = lesson_texture::pixels;

    LoadedModel loaded_model{};
    const std::span<const std::uint8_t> model_file{
        lesson_asset::bytes.data(), lesson_asset::bytes.size()};
    if(!loaded_model.load(model_file)) {
        dbglog(DBG_ERROR,
               "maishuji: DCM1 model validation or decode failed\n");
        return 1;
    }

    maishuji::Pvr pvr;
    maishuji::Status status = pvr.initialize();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: PVR initialization failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }

    maishuji::Texture texture;
    status = texture.allocate(pvr, lesson_texture::width,
                              lesson_texture::height);
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: model texture allocation failed: %s\n",
               maishuji::status_name(status));
        (void)pvr.shutdown();
        return 1;
    }

    status = texture.upload(std::span<const std::uint16_t>{texture_pixels});
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: model texture upload failed: %s\n",
               maishuji::status_name(status));
        (void)texture.release();
        (void)pvr.shutdown();
        return 1;
    }

    const maishuji::TexturedMesh model = loaded_model.mesh();
    constexpr maishuji::Camera camera = model_camera();

    for(int frame = 0; frame < animation_frames; ++frame) {
#ifdef MAISHUJI_HUMAN_MODEL
        status = run_frame(pvr, texture, model, camera,
                           capture_transform());
#else
        status = run_frame(pvr, texture, model, camera,
                           transform_for_frame(frame));
#endif
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR,
                   "maishuji: model loader frame %d failed: %s\n", frame,
                   maishuji::status_name(status));
            (void)texture.release();
            (void)pvr.shutdown();
            return 1;
        }
    }

    dbglog(DBG_NOTICE,
           "maishuji: model loader passed (DCM1; %u vertices; %u triangles; %ux%u ARGB4444)\n",
           loaded_model.vertex_count, loaded_model.index_count / 3,
           lesson_texture::width, lesson_texture::height);

    for(int frame = 0; frame < capture_hold_frames; ++frame) {
        status = run_frame(pvr, texture, model, camera,
                           capture_transform());
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR,
                   "maishuji: model loader capture hold frame %d failed: %s\n",
                   frame, maishuji::status_name(status));
            (void)texture.release();
            (void)pvr.shutdown();
            return 1;
        }
    }

    status = texture.release();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: model texture release failed: %s\n",
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
