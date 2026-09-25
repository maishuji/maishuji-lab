#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace maishuji {

enum class List : std::uint8_t {
    Opaque,
    PunchThrough,
    Translucent,
};

enum class Culling : std::uint8_t {
    None,
    Clockwise,
    CounterClockwise,
};

struct Color {
    std::uint8_t red = 255;
    std::uint8_t green = 255;
    std::uint8_t blue = 255;
    std::uint8_t alpha = 255;

    constexpr std::uint32_t argb() const noexcept {
        return (static_cast<std::uint32_t>(alpha) << 24) |
               (static_cast<std::uint32_t>(red) << 16) |
               (static_cast<std::uint32_t>(green) << 8) |
               static_cast<std::uint32_t>(blue);
    }
};

struct Vertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 1.0f;
    Color color{};
};

struct Triangle {
    Vertex first{};
    Vertex second{};
    Vertex third{};
};

struct Quad {
    Vertex top_left{};
    Vertex bottom_left{};
    Vertex top_right{};
    Vertex bottom_right{};
};

struct PrimitiveConfiguration {
    Culling culling = Culling::None;
};

struct TexturedVertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 1.0f;
    float u = 0.0f;
    float v = 0.0f;
    Color color{};
};

struct TexturedQuad {
    TexturedVertex top_left{};
    TexturedVertex bottom_left{};
    TexturedVertex top_right{};
    TexturedVertex bottom_right{};
};

struct Configuration {
    bool enable_opaque = true;
    bool enable_punch_through = true;
    bool enable_translucent = true;

    std::size_t vertex_buffer_bytes = 512 * 1024;
    bool vertex_dma = false;
    bool fsaa = false;
    bool translucent_autosort = true;
    std::uint32_t opb_overflow = 3;
    bool vertex_buffer_double_buffering = true;
};

enum class Status : std::uint8_t {
    Success,
    AlreadyInitialized,
    NotInitialized,
    InvalidConfiguration,
    BackendInitializationFailed,
    BackendShutdownFailed,
    FrameAlreadyActive,
    FrameActive,
    FrameNotActive,
    WaitReadyFailed,
    SceneBeginFailed,
    SceneFinishFailed,
    RenderWaitFailed,
    RenderListAlreadyActive,
    RenderListAlreadyFinished,
    RenderListActive,
    RenderListNotActive,
    RenderListDisabled,
    RenderListBeginFailed,
    RenderListFinishFailed,
    PrimitiveSubmissionFailed,
    TextureAlreadyAllocated,
    TextureNotAllocated,
    TextureInvalidDimensions,
    TextureAllocationFailed,
    TextureInvalidData,
    TextureUploadFailed,
    TextureContextMismatch,
};

constexpr bool succeeded(Status status) noexcept {
    return status == Status::Success;
}

constexpr bool failed(Status status) noexcept {
    return !succeeded(status);
}

const char *status_name(Status status) noexcept;

class Frame;
class RenderList;
class Texture;

namespace detail {
struct Backend;
}

class Pvr {
public:
    Pvr() noexcept;
    ~Pvr() noexcept;

    Pvr(const Pvr &) = delete;
    Pvr &operator=(const Pvr &) = delete;
    Pvr(Pvr &&) = delete;
    Pvr &operator=(Pvr &&) = delete;

    Status initialize(const Configuration &configuration = {}) noexcept;
    Status begin_frame(Frame &frame) noexcept;
    Status wait_render_done() noexcept;
    Status shutdown() noexcept;

    bool initialized() const noexcept {
        return initialized_;
    }

    const Configuration &configuration() const noexcept {
        return configuration_;
    }

private:
    friend class Frame;
    friend class RenderList;
    friend class Texture;

    const detail::Backend *backend_;
    Configuration configuration_{};
    Frame *active_frame_ = nullptr;
    bool initialized_ = false;
};

class Texture {
public:
    Texture() noexcept = default;
    ~Texture() noexcept;

    Texture(const Texture &) = delete;
    Texture &operator=(const Texture &) = delete;
    Texture(Texture &&other) noexcept;
    Texture &operator=(Texture &&other) noexcept;

    Status allocate(Pvr &pvr, std::uint16_t width,
                    std::uint16_t height) noexcept;
    Status upload(std::span<const std::uint16_t> pixels) noexcept;
    Status release() noexcept;

    bool allocated() const noexcept {
        return allocated_;
    }

    std::uint16_t width() const noexcept {
        return width_;
    }

    std::uint16_t height() const noexcept {
        return height_;
    }

private:
    friend class RenderList;

    Pvr *owner_ = nullptr;
    std::uintptr_t handle_ = 0;
    std::uint16_t width_ = 0;
    std::uint16_t height_ = 0;
    bool allocated_ = false;
};

class Frame {
public:
    Frame() noexcept = default;
    ~Frame() noexcept;

    Frame(const Frame &) = delete;
    Frame &operator=(const Frame &) = delete;
    Frame(Frame &&) = delete;
    Frame &operator=(Frame &&) = delete;

    Status begin_list(RenderList &list, List list_type) noexcept;
    Status finish() noexcept;

    bool active() const noexcept {
        return active_;
    }

    Status completion_status() const noexcept {
        return last_status_;
    }

private:
    friend class Pvr;
    friend class RenderList;

    Status close_from_destructor() noexcept;

    Pvr *owner_ = nullptr;
    RenderList *active_list_ = nullptr;
    std::uint8_t started_lists_ = 0;
    bool active_ = false;
    Status last_status_ = Status::Success;
};

class RenderList {
public:
    RenderList() noexcept = default;
    ~RenderList() noexcept;

    RenderList(const RenderList &) = delete;
    RenderList &operator=(const RenderList &) = delete;
    RenderList(RenderList &&) = delete;
    RenderList &operator=(RenderList &&) = delete;

    Status finish() noexcept;
    Status submit(const Triangle &triangle,
                  const PrimitiveConfiguration &configuration = {}) noexcept;
    Status submit(const Quad &quad,
                  const PrimitiveConfiguration &configuration = {}) noexcept;
    Status submit(const Texture &texture, const TexturedQuad &quad,
                  const PrimitiveConfiguration &configuration = {}) noexcept;

    bool active() const noexcept {
        return active_;
    }

    List type() const noexcept {
        return list_type_;
    }

    Status completion_status() const noexcept {
        return last_status_;
    }

private:
    friend class Frame;

    Frame *owner_ = nullptr;
    List list_type_ = List::Opaque;
    bool active_ = false;
    Status last_status_ = Status::Success;
};

} // namespace maishuji
