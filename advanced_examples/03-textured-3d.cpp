#include "maishuji/mesh.hpp"
#include "maishuji/pvr.hpp"

#include "assets/pvr_asset.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <kos.h>

namespace {

constexpr int frames = 180;
constexpr int capture_hold_frames = 900;
constexpr float screen_width = 640.0f;
constexpr float screen_height = 480.0f;
constexpr float cube_half_extent = 0.9f;

struct PvrImage {
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::span<const std::uint8_t> payload{};
};

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

bool has_fourcc(std::span<const std::uint8_t> bytes,
                const char (&fourcc)[5]) noexcept {
    for(std::size_t index = 0; index < 4; ++index) {
        if(bytes[index] != static_cast<std::uint8_t>(fourcc[index]))
            return false;
    }
    return true;
}

bool parse_pvr(std::span<const std::uint8_t> file, PvrImage &image) noexcept {
    if(file.size() < 16 || !has_fourcc(file, "PVRT"))
        return false;

    const std::uint32_t chunk_size = read_le32(file, 4);
    if(chunk_size != file.size() &&
       static_cast<std::size_t>(chunk_size) + 8 != file.size())
        return false;

    const std::uint32_t type = read_le32(file, 8);
    const std::uint8_t texture_type =
        static_cast<std::uint8_t>((type >> 8) & 0xffu);
    const std::uint8_t pixel_format = static_cast<std::uint8_t>(type & 0xffu);
    if(texture_type != 1 || pixel_format != 2)
        return false;

    const std::uint16_t width = read_le16(file, 12);
    const std::uint16_t height = read_le16(file, 14);
    if(width == 0 || width != height ||
       (width & static_cast<std::uint16_t>(width - 1)) != 0)
        return false;

    const std::size_t payload_bytes =
        static_cast<std::size_t>(width) * height * sizeof(std::uint16_t);
    if(file.size() < 16 + payload_bytes)
        return false;

    image = {width, height, file.subspan(16, payload_bytes)};
    return true;
}

std::size_t morton_index(std::uint16_t x, std::uint16_t y) noexcept {
    std::uint32_t index = 0;
    for(unsigned bit = 0; bit < 16; ++bit) {
        index |= ((static_cast<std::uint32_t>(x) >> bit) & 1u) <<
                 (bit * 2 + 1);
        index |= ((static_cast<std::uint32_t>(y) >> bit) & 1u) <<
                 (bit * 2);
    }
    return index;
}

bool decode_pvr(const PvrImage &image,
                std::span<std::uint16_t> destination) noexcept {
    const std::size_t pixel_count =
        static_cast<std::size_t>(image.width) * image.height;
    if(destination.size() != pixel_count ||
       image.payload.size() != pixel_count * sizeof(std::uint16_t))
        return false;

    for(std::uint16_t y = 0; y < image.height; ++y) {
        for(std::uint16_t x = 0; x < image.width; ++x) {
            const std::size_t source_pixel = morton_index(x, y);
            const std::size_t source_byte = source_pixel * sizeof(std::uint16_t);
            destination[static_cast<std::size_t>(y) * image.width + x] =
                static_cast<std::uint16_t>(
                    static_cast<std::uint16_t>(image.payload[source_byte]) |
                    (static_cast<std::uint16_t>(image.payload[source_byte + 1])
                     << 8));
        }
    }
    return true;
}

constexpr maishuji::Quad make_background() noexcept {
    constexpr maishuji::Color background{8, 16, 36, 255};
    return {
        {0.0f, 0.0f, 0.05f, background},
        {0.0f, screen_height, 0.05f, background},
        {screen_width, 0.0f, 0.05f, background},
        {screen_width, screen_height, 0.05f, background},
    };
}

constexpr std::array<maishuji::TexturedMeshVertex, 24>
make_cube_vertices() noexcept {
    constexpr maishuji::Color white{255, 255, 255, 255};
    constexpr float h = cube_half_extent;
    return {{
        // Front (+Z).
        {{-h, -h, h}, 0.0f, 0.0f, white},
        {{-h, h, h}, 0.0f, 1.0f, white},
        {{h, -h, h}, 1.0f, 0.0f, white},
        {{h, h, h}, 1.0f, 1.0f, white},
        // Back (-Z).
        {{h, -h, -h}, 0.0f, 0.0f, white},
        {{h, h, -h}, 0.0f, 1.0f, white},
        {{-h, -h, -h}, 1.0f, 0.0f, white},
        {{-h, h, -h}, 1.0f, 1.0f, white},
        // Left (-X).
        {{-h, -h, -h}, 0.0f, 0.0f, white},
        {{-h, h, -h}, 0.0f, 1.0f, white},
        {{-h, -h, h}, 1.0f, 0.0f, white},
        {{-h, h, h}, 1.0f, 1.0f, white},
        // Right (+X).
        {{h, -h, h}, 0.0f, 0.0f, white},
        {{h, h, h}, 0.0f, 1.0f, white},
        {{h, -h, -h}, 1.0f, 0.0f, white},
        {{h, h, -h}, 1.0f, 1.0f, white},
        // Top (-Y).
        {{-h, -h, -h}, 0.0f, 0.0f, white},
        {{-h, -h, h}, 0.0f, 1.0f, white},
        {{h, -h, -h}, 1.0f, 0.0f, white},
        {{h, -h, h}, 1.0f, 1.0f, white},
        // Bottom (+Y).
        {{-h, h, h}, 0.0f, 0.0f, white},
        {{-h, h, -h}, 0.0f, 1.0f, white},
        {{h, h, h}, 1.0f, 0.0f, white},
        {{h, h, -h}, 1.0f, 1.0f, white},
    }};
}

constexpr std::array<std::uint16_t, 36> make_cube_indices() noexcept {
    std::array<std::uint16_t, 36> indices{};
    for(std::uint16_t face = 0; face < 6; ++face) {
        const std::uint16_t base = face * 4;
        const std::size_t offset = face * 6;
        indices[offset + 0] = base + 0;
        indices[offset + 1] = base + 1;
        indices[offset + 2] = base + 2;
        indices[offset + 3] = base + 1;
        indices[offset + 4] = base + 3;
        indices[offset + 5] = base + 2;
    }
    return indices;
}

maishuji::Transform transform_for_frame(int frame) noexcept {
    const float angle = static_cast<float>(frame) * 0.035f;
    return {
        {0.0f, 0.0f, 0.0f},
        {0.32f, angle, angle * 0.45f},
        {1.0f, 1.0f, 1.0f},
    };
}

constexpr maishuji::Transform capture_transform() noexcept {
    return {
        {0.0f, 0.0f, 0.0f},
        {0.28f, 0.65f, -0.28f},
        {1.0f, 1.0f, 1.0f},
    };
}

maishuji::Status run_frame(
    maishuji::Pvr &pvr, const maishuji::Texture &texture,
    const maishuji::Quad &background, const maishuji::TexturedMesh &mesh,
    const maishuji::Camera &camera,
    const maishuji::Transform &transform) noexcept {
    maishuji::Frame frame;
    maishuji::Status status = pvr.begin_frame(frame);
    if(maishuji::failed(status))
        return status;

    maishuji::RenderList opaque;
    status = frame.begin_list(opaque, maishuji::List::Opaque);
    if(maishuji::failed(status))
        return status;
    status = opaque.submit(background);
    if(maishuji::failed(status))
        return status;
    status = opaque.finish();
    if(maishuji::failed(status))
        return status;

    maishuji::RenderList textured;
    status = frame.begin_list(textured, maishuji::List::PunchThrough);
    if(maishuji::failed(status))
        return status;
    status = textured.submit(texture, mesh, camera, transform,
                            maishuji::Viewport{screen_width, screen_height});
    if(maishuji::failed(status))
        return status;
    status = textured.finish();
    if(maishuji::failed(status))
        return status;

    return frame.finish();
}

} // namespace

int main() {
    static_assert(maishuji::pvr_asset::bytes.size() == 131088);
    alignas(32) static std::array<std::uint16_t,
                                  maishuji::pvr_asset::width *
                                      maishuji::pvr_asset::height>
        pixels{};

    PvrImage parsed_image{};
    const std::span<const std::uint8_t> file{
        maishuji::pvr_asset::bytes.data(),
        maishuji::pvr_asset::bytes.size()};
    if(!parse_pvr(file, parsed_image) ||
       !decode_pvr(parsed_image, std::span<std::uint16_t>{pixels})) {
        dbglog(DBG_ERROR,
               "maishuji: textured 3D asset parse or Morton decode failed\n");
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
    status = texture.allocate(pvr, parsed_image.width, parsed_image.height);
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR,
               "maishuji: textured 3D texture allocation failed: %s\n",
               maishuji::status_name(status));
        (void)pvr.shutdown();
        return 1;
    }

    status = texture.upload(std::span<const std::uint16_t>{pixels});
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: textured 3D texture upload failed: %s\n",
               maishuji::status_name(status));
        (void)texture.release();
        (void)pvr.shutdown();
        return 1;
    }

    constexpr auto cube_vertices = make_cube_vertices();
    constexpr auto cube_indices = make_cube_indices();
    const maishuji::TexturedMesh cube{cube_vertices, cube_indices};
    constexpr maishuji::Quad background = make_background();
    const maishuji::Camera camera{
        {0.0f, 0.0f, 4.5f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        1.04719755f,
        4.0f / 3.0f,
        0.1f,
        100.0f,
    };

    for(int frame = 0; frame < frames; ++frame) {
        status = run_frame(pvr, texture, background, cube, camera,
                           transform_for_frame(frame));
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR,
                   "maishuji: textured 3D frame %d failed: %s\n",
                   frame, maishuji::status_name(status));
            (void)texture.release();
            (void)pvr.shutdown();
            return 1;
        }
    }

    dbglog(DBG_NOTICE,
           "maishuji: textured 3D passed (PVRT ARGB4444; rotating cube; 12 triangles)\n");

    for(int frame = 0; frame < capture_hold_frames; ++frame) {
        status = run_frame(pvr, texture, background, cube, camera,
                           capture_transform());
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR,
                   "maishuji: textured 3D capture hold frame %d failed: %s\n",
                   frame, maishuji::status_name(status));
            (void)texture.release();
            (void)pvr.shutdown();
            return 1;
        }
    }

    status = texture.release();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR,
               "maishuji: textured 3D texture release failed: %s\n",
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
