#include "maishuji/mesh.hpp"
#include "maishuji/pvr.hpp"

#include "assets/terrain_asset.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

#include <kos.h>

namespace {

constexpr std::size_t terrain_points = 9;
constexpr std::size_t terrain_cells = terrain_points - 1;
constexpr std::size_t terrain_vertex_count =
    terrain_points * terrain_points;
constexpr std::size_t terrain_index_count =
    terrain_cells * terrain_cells * 6;
constexpr float terrain_spacing = 1.25f;
constexpr float height_scale = 0.18f;
constexpr float texture_repeat = 4.0f;
constexpr int animation_frames = 240;
constexpr int capture_hold_frames = 900;
constexpr float screen_width = 640.0f;
constexpr float screen_height = 480.0f;

// A compact 9x9 heightmap. Each value is one sample, not one polygon.
constexpr std::array<std::uint8_t, terrain_vertex_count> heightmap{{
    0, 1, 2, 3, 2, 1, 0, 0, 1,
    1, 2, 4, 6, 5, 3, 1, 1, 0,
    2, 4, 7, 8, 7, 4, 2, 1, 1,
    3, 6, 8, 6, 5, 3, 2, 2, 1,
    2, 5, 7, 6, 4, 2, 1, 1, 0,
    1, 3, 5, 4, 3, 1, 0, 0, 1,
    0, 1, 3, 2, 1, 0, 0, 1, 2,
    0, 1, 2, 1, 0, 0, 1, 2, 3,
    1, 1, 2, 1, 0, 1, 2, 3, 4,
}};

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

maishuji::Color color_for_height(std::uint8_t value) noexcept {
    const std::uint16_t height = value;
    return {
        static_cast<std::uint8_t>(160 + height * 10),
        static_cast<std::uint8_t>(145 + height * 10),
        static_cast<std::uint8_t>(128 + height * 8),
        255,
    };
}

void build_terrain(
    std::array<maishuji::TexturedMeshVertex, terrain_vertex_count> &vertices,
    std::array<std::uint16_t, terrain_index_count> &indices) noexcept {
    for(std::size_t z = 0; z < terrain_points; ++z) {
        for(std::size_t x = 0; x < terrain_points; ++x) {
            const std::size_t index = z * terrain_points + x;
            const std::uint8_t height = heightmap[index];
            const float world_x =
                (static_cast<float>(x) -
                 static_cast<float>(terrain_cells) * 0.5f) *
                terrain_spacing;
            const float world_z =
                (static_cast<float>(z) -
                 static_cast<float>(terrain_cells) * 0.5f) *
                terrain_spacing;
            const float u =
                static_cast<float>(x) /
                static_cast<float>(terrain_cells) * texture_repeat;
            const float v =
                static_cast<float>(z) /
                static_cast<float>(terrain_cells) * texture_repeat;

            vertices[index] = {
                {world_x, static_cast<float>(height) * height_scale - 0.35f,
                 world_z},
                u,
                v,
                color_for_height(height),
            };
        }
    }

    std::size_t output = 0;
    for(std::size_t z = 0; z < terrain_cells; ++z) {
        for(std::size_t x = 0; x < terrain_cells; ++x) {
            const std::uint16_t top_left =
                static_cast<std::uint16_t>(z * terrain_points + x);
            const std::uint16_t top_right = top_left + 1;
            const std::uint16_t bottom_left =
                top_left + static_cast<std::uint16_t>(terrain_points);
            const std::uint16_t bottom_right = bottom_left + 1;

            indices[output++] = top_left;
            indices[output++] = bottom_left;
            indices[output++] = top_right;
            indices[output++] = bottom_left;
            indices[output++] = bottom_right;
            indices[output++] = top_right;
        }
    }
}

maishuji::Camera camera_for_frame(int frame) noexcept {
    const float angle = static_cast<float>(frame) * 0.012f;
    return {
        {std::sin(angle) * 1.8f, 5.4f, 8.6f + std::cos(angle) * 0.6f},
        {0.0f, 0.25f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        0.95f,
        4.0f / 3.0f,
        0.1f,
        100.0f,
    };
}

maishuji::Camera capture_camera() noexcept {
    return {
        {5.8f, 5.8f, 7.8f},
        {0.0f, 0.25f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        0.95f,
        4.0f / 3.0f,
        0.1f,
        100.0f,
    };
}

maishuji::Status run_frame(
    maishuji::Pvr &pvr, const maishuji::Texture &texture,
    const maishuji::Quad &background, const maishuji::TexturedMesh &terrain,
    const maishuji::Camera &camera) noexcept {
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
    status = opaque.submit(texture, terrain, camera, maishuji::Transform{},
                           maishuji::Viewport{screen_width, screen_height});
    if(maishuji::failed(status))
        return status;
    status = opaque.finish();
    if(maishuji::failed(status))
        return status;

    return frame.finish();
}

} // namespace

int main() {
    static_assert(maishuji::terrain_asset::bytes.size() == 131088);
    alignas(32) static std::array<std::uint16_t,
                                  maishuji::terrain_asset::width *
                                      maishuji::terrain_asset::height>
        pixels{};

    PvrImage parsed_image{};
    const std::span<const std::uint8_t> file{
        maishuji::terrain_asset::bytes.data(),
        maishuji::terrain_asset::bytes.size()};
    if(!parse_pvr(file, parsed_image) ||
       !decode_pvr(parsed_image, std::span<std::uint16_t>{pixels})) {
        dbglog(DBG_ERROR,
               "maishuji: heightmapped terrain asset decode failed\n");
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
               "maishuji: heightmapped terrain texture allocation failed: %s\n",
               maishuji::status_name(status));
        (void)pvr.shutdown();
        return 1;
    }

    status = texture.upload(std::span<const std::uint16_t>{pixels});
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR,
               "maishuji: heightmapped terrain texture upload failed: %s\n",
               maishuji::status_name(status));
        (void)texture.release();
        (void)pvr.shutdown();
        return 1;
    }

    std::array<maishuji::TexturedMeshVertex, terrain_vertex_count> vertices{};
    std::array<std::uint16_t, terrain_index_count> indices{};
    build_terrain(vertices, indices);
    const maishuji::TexturedMesh terrain{vertices, indices};

    constexpr maishuji::Quad background = make_background();

    for(int frame = 0; frame < animation_frames; ++frame) {
        status = run_frame(pvr, texture, background, terrain,
                           camera_for_frame(frame));
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR,
                   "maishuji: heightmapped terrain frame %d failed: %s\n",
                   frame, maishuji::status_name(status));
            (void)texture.release();
            (void)pvr.shutdown();
            return 1;
        }
    }

    dbglog(
        DBG_NOTICE,
        "maishuji: heightmapped terrain passed (256x256 ARGB4444; 81 vertices; 128 triangles)\n");

    for(int frame = 0; frame < capture_hold_frames; ++frame) {
        status = run_frame(pvr, texture, background, terrain,
                           capture_camera());
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR,
                   "maishuji: heightmapped terrain capture hold frame %d failed: %s\n",
                   frame, maishuji::status_name(status));
            (void)texture.release();
            (void)pvr.shutdown();
            return 1;
        }
    }

    status = texture.release();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR,
               "maishuji: heightmapped terrain texture release failed: %s\n",
               maishuji::status_name(status));
        (void)pvr.shutdown();
        return 1;
    }

    status = pvr.shutdown();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: PVR shutdown failed\n");
        return 1;
    }
    return 0;
}
