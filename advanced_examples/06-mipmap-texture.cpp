#include "maishuji/mesh.hpp"
#include "maishuji/pvr.hpp"

#include "assets/mipmap_asset.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <span>

#include <kos.h>

namespace {

constexpr std::size_t mipmap_upload_bytes = 174768;
constexpr int animation_frames = 180;
constexpr int capture_hold_frames = 900;
constexpr float screen_width = 640.0f;
constexpr float screen_height = 480.0f;
constexpr float camera_aspect = 4.0f / 3.0f;

struct PvrImage {
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::span<const std::uint8_t> payload{};
};

constexpr std::size_t texture_bytes(std::uint16_t width,
                                    std::uint16_t height) noexcept {
    return static_cast<std::size_t>(width) * height * sizeof(std::uint16_t);
}

constexpr std::size_t mipmap_storage_bytes(std::uint16_t width,
                                           std::uint16_t height) noexcept {
    // KOS expects six leading bytes before the smallest mip level.
    std::size_t bytes = 6;
    while(true) {
        bytes += texture_bytes(width, height);
        if(width == 1 && height == 1)
            return bytes;
        width = width > 1 ? static_cast<std::uint16_t>(width / 2) : 1;
        height = height > 1 ? static_cast<std::uint16_t>(height / 2) : 1;
    }
}

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

bool parse_mipmap_pvr(std::span<const std::uint8_t> file,
                      PvrImage &image) noexcept {
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
    if(texture_type != 2 || pixel_format != 2)
        return false;

    const std::uint16_t width = read_le16(file, 12);
    const std::uint16_t height = read_le16(file, 14);
    if(width == 0 || width != height ||
       (width & static_cast<std::uint16_t>(width - 1)) != 0)
        return false;

    // pvrtex omits four bytes from the six-byte leading hardware padding in
    // the PVRT file. The upload buffer restores those bytes below.
    const std::size_t file_payload_bytes = mipmap_storage_bytes(width, height) - 4;
    if(file.size() != 16 + file_payload_bytes)
        return false;

    image = {width, height, file.subspan(16, file_payload_bytes)};
    return true;
}

constexpr maishuji::Quad make_background() noexcept {
    constexpr maishuji::Color background{8, 12, 28, 255};
    return {
        {0.0f, 0.0f, 0.05f, background},
        {0.0f, screen_height, 0.05f, background},
        {screen_width, 0.0f, 0.05f, background},
        {screen_width, screen_height, 0.05f, background},
    };
}

constexpr std::array<maishuji::TexturedMeshVertex, 4>
make_strip(float left, float right) noexcept {
    constexpr maishuji::Color white{255, 255, 255, 255};
    return {{
        {{left, -0.9f, 1.0f}, 0.0f, 0.0f, white},
        {{left, -0.9f, 42.0f}, 0.0f, 32.0f, white},
        {{right, -0.9f, 1.0f}, 32.0f, 0.0f, white},
        {{right, -0.9f, 42.0f}, 32.0f, 32.0f, white},
    }};
}

constexpr std::array<std::uint16_t, 6> strip_indices{0, 1, 2, 1, 3, 2};

maishuji::Camera camera_for_frame(int frame) noexcept {
    const float angle = static_cast<float>(frame) * 0.012f;
    return {
        {std::sin(angle) * 1.4f, 3.4f, -5.0f + std::cos(angle) * 0.8f},
        {0.0f, -0.55f, 16.0f},
        {0.0f, 1.0f, 0.0f},
        0.82f,
        camera_aspect,
        0.1f,
        100.0f,
    };
}

constexpr maishuji::Camera capture_camera() noexcept {
    return {
        {0.0f, 3.4f, -5.0f},
        {0.0f, -0.55f, 16.0f},
        {0.0f, 1.0f, 0.0f},
        0.82f,
        camera_aspect,
        0.1f,
        100.0f,
    };
}

maishuji::Status run_frame(
    maishuji::Pvr &pvr, const maishuji::Texture &texture,
    const maishuji::Quad &background,
    const maishuji::TexturedMesh &base_strip,
    const maishuji::TexturedMesh &mipmap_strip,
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

    const maishuji::Viewport viewport{screen_width, screen_height};
    const maishuji::TextureSampling base_sampling{
        maishuji::TextureFilter::Bilinear, false};
    const maishuji::TextureSampling mipmap_sampling{
        maishuji::TextureFilter::Bilinear, true};
    status = opaque.submit(texture, base_strip, camera,
                           maishuji::Transform{}, viewport, base_sampling);
    if(maishuji::failed(status))
        return status;
    status = opaque.submit(texture, mipmap_strip, camera,
                           maishuji::Transform{}, viewport, mipmap_sampling);
    if(maishuji::failed(status))
        return status;

    status = opaque.finish();
    if(maishuji::failed(status))
        return status;
    return frame.finish();
}

} // namespace

int main() {
    static_assert(maishuji::mipmap_asset::bytes.size() == 174780);
    static_assert(mipmap_storage_bytes(256, 256) == mipmap_upload_bytes);

    alignas(32) static std::array<std::uint8_t, mipmap_upload_bytes>
        upload_data{};
    const std::span<const std::uint8_t> file{
        maishuji::mipmap_asset::bytes.data(),
        maishuji::mipmap_asset::bytes.size()};
    PvrImage parsed_image{};
    if(!parse_mipmap_pvr(file, parsed_image) ||
       parsed_image.width != maishuji::mipmap_asset::width ||
       parsed_image.height != maishuji::mipmap_asset::height ||
       parsed_image.payload.size() + 4 != upload_data.size()) {
        dbglog(DBG_ERROR, "maishuji: mipmap texture asset decode failed\n");
        return 1;
    }

    std::copy(parsed_image.payload.begin(), parsed_image.payload.end(),
              upload_data.begin() + 4);

    maishuji::Pvr pvr;
    maishuji::Status status = pvr.initialize();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: PVR initialization failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }

    maishuji::Texture texture;
    status = texture.allocate_mipmapped(pvr, parsed_image.width,
                                        parsed_image.height);
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR,
               "maishuji: mipmap texture allocation failed: %s\n",
               maishuji::status_name(status));
        (void)pvr.shutdown();
        return 1;
    }
    status = texture.upload_mip_chain(
        std::span<const std::uint8_t>{upload_data});
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR,
               "maishuji: mipmap texture upload failed: %s\n",
               maishuji::status_name(status));
        (void)texture.release();
        (void)pvr.shutdown();
        return 1;
    }

    constexpr auto base_vertices = make_strip(-4.2f, -0.25f);
    constexpr auto mipmap_vertices = make_strip(0.25f, 4.2f);
    const maishuji::TexturedMesh base_strip{base_vertices, strip_indices};
    const maishuji::TexturedMesh mipmap_strip{mipmap_vertices, strip_indices};
    constexpr maishuji::Quad background = make_background();

    for(int frame = 0; frame < animation_frames; ++frame) {
        status = run_frame(pvr, texture, background, base_strip, mipmap_strip,
                           camera_for_frame(frame));
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR,
                   "maishuji: mipmap texture frame %d failed: %s\n", frame,
                   maishuji::status_name(status));
            (void)texture.release();
            (void)pvr.shutdown();
            return 1;
        }
    }

    dbglog(DBG_NOTICE,
           "maishuji: mipmap texture passed (9 levels; 174768 bytes; bilinear filtering)\n");

    for(int frame = 0; frame < capture_hold_frames; ++frame) {
        status = run_frame(pvr, texture, background, base_strip, mipmap_strip,
                           capture_camera());
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR,
                   "maishuji: mipmap texture capture frame %d failed: %s\n",
                   frame, maishuji::status_name(status));
            (void)texture.release();
            (void)pvr.shutdown();
            return 1;
        }
    }

    status = texture.release();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: mipmap texture release failed: %s\n",
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
