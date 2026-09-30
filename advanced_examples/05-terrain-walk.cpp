#include "maishuji/mesh.hpp"
#include "maishuji/pvr.hpp"

#include "assets/terrain_asset.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

#include <dc/maple.h>
#include <dc/maple/controller.h>

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

float sample_height(float world_x, float world_z) noexcept {
    const float grid_x = std::clamp(
        world_x / terrain_spacing +
            static_cast<float>(terrain_cells) * 0.5f,
        0.0f, static_cast<float>(terrain_cells));
    const float grid_z = std::clamp(
        world_z / terrain_spacing +
            static_cast<float>(terrain_cells) * 0.5f,
        0.0f, static_cast<float>(terrain_cells));

    const std::size_t x0 = std::min(
        static_cast<std::size_t>(grid_x), terrain_cells - 1);
    const std::size_t z0 = std::min(
        static_cast<std::size_t>(grid_z), terrain_cells - 1);
    const std::size_t x1 = x0 + 1;
    const std::size_t z1 = z0 + 1;
    const float tx = grid_x - static_cast<float>(x0);
    const float tz = grid_z - static_cast<float>(z0);

    const auto height_at = [](std::size_t x, std::size_t z) noexcept {
        return static_cast<float>(heightmap[z * terrain_points + x]) *
                   height_scale -
               0.35f;
    };
    const float top = height_at(x0, z0) +
                      (height_at(x1, z0) - height_at(x0, z0)) * tx;
    const float bottom = height_at(x0, z1) +
                         (height_at(x1, z1) - height_at(x0, z1)) * tx;
    return top + (bottom - top) * tz;
}

struct Player {
    maishuji::Vec3 position{};
};

struct MovementInput {
    float x = 0.0f;
    float z = 0.0f;
    bool active = false;
};

void clamp_player(Player &player) noexcept {
    constexpr float half_extent =
        static_cast<float>(terrain_cells) * terrain_spacing * 0.5f;
    constexpr float margin = 0.35f;
    player.position.x = std::clamp(
        player.position.x, -half_extent + margin, half_extent - margin);
    player.position.z = std::clamp(
        player.position.z, -half_extent + margin, half_extent - margin);
    player.position.y =
        sample_height(player.position.x, player.position.z);
}

MovementInput read_controller(maple_device_t *device) noexcept {
    if(device == nullptr)
        return {};

    const auto *state =
        static_cast<const cont_state_t *>(maple_dev_status(device));
    if(state == nullptr)
        return {};

    float x = 0.0f;
    float z = 0.0f;
    if((state->buttons & CONT_DPAD_LEFT) != 0)
        x -= 1.0f;
    if((state->buttons & CONT_DPAD_RIGHT) != 0)
        x += 1.0f;
    if((state->buttons & CONT_DPAD_UP) != 0)
        z -= 1.0f;
    if((state->buttons & CONT_DPAD_DOWN) != 0)
        z += 1.0f;

    constexpr float joystick_dead_zone = 20.0f;
    if(std::fabs(static_cast<float>(state->joyx)) >
       joystick_dead_zone)
        x += static_cast<float>(state->joyx) / 127.0f;
    if(std::fabs(static_cast<float>(state->joyy)) >
       joystick_dead_zone)
        z += static_cast<float>(state->joyy) / 127.0f;

    const float length = std::sqrt(x * x + z * z);
    if(length <= 0.05f)
        return {};
    if(length > 1.0f) {
        x /= length;
        z /= length;
    }
    return {x, z, true};
}

Player scripted_player(int frame) noexcept {
    const float time = static_cast<float>(frame) * 0.018f;
    Player player{
        {std::sin(time) * 2.4f, 0.0f,
         std::cos(time * 0.72f) * 2.4f},
    };
    clamp_player(player);
    return player;
}

Player capture_player() noexcept {
    Player player{{-0.8f, 0.0f, -0.6f}};
    clamp_player(player);
    return player;
}

void apply_input(Player &player, MovementInput input) noexcept {
    constexpr float movement_speed = 0.11f;
    player.position.x += input.x * movement_speed;
    player.position.z += input.z * movement_speed;
    clamp_player(player);
}

maishuji::Camera camera_for_player(const maishuji::Vec3 &player) noexcept {
    return {
        {player.x + 4.8f, player.y + 4.4f, player.z + 6.4f},
        {player.x, player.y + 0.25f, player.z},
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
    const Player &player) noexcept {
    const maishuji::Camera camera = camera_for_player(player.position);
    const std::array<maishuji::MeshVertex, 3> marker_vertices{
        maishuji::MeshVertex{
            {player.position.x, player.position.y + 0.08f,
             player.position.z - 0.45f},
            {255, 48, 255, 255}},
        maishuji::MeshVertex{
            {player.position.x - 0.40f, player.position.y + 0.08f,
             player.position.z + 0.32f},
            {255, 48, 255, 255}},
        maishuji::MeshVertex{
            {player.position.x + 0.40f, player.position.y + 0.08f,
             player.position.z + 0.32f},
            {255, 48, 255, 255}},
    };
    constexpr std::array<std::uint16_t, 3> marker_indices{0, 1, 2};
    const maishuji::Mesh marker{marker_vertices, marker_indices};

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
    status = opaque.submit(marker, camera, maishuji::Transform{},
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

    maple_device_t *controller =
        maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    Player player = scripted_player(0);
    bool scripted_mode = true;

    for(int frame = 0; frame < animation_frames; ++frame) {
        const MovementInput input = read_controller(controller);
        if(input.active)
            scripted_mode = false;
        if(scripted_mode)
            player = scripted_player(frame);
        else
            apply_input(player, input);

        status = run_frame(pvr, texture, background, terrain, player);
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR,
                   "maishuji: terrain walk frame %d failed: %s\n",
                   frame, maishuji::status_name(status));
            (void)texture.release();
            (void)pvr.shutdown();
            return 1;
        }
    }

    dbglog(
        DBG_NOTICE,
        "maishuji: terrain walk passed (controller movement; height sampling; 81 vertices; 128 triangles)\n");

    const Player capture_player_state = capture_player();
    for(int frame = 0; frame < capture_hold_frames; ++frame) {
        status = run_frame(pvr, texture, background, terrain,
                           capture_player_state);
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR,
                   "maishuji: terrain walk capture hold frame %d failed: %s\n",
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
