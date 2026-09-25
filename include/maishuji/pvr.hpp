#pragma once

#include <cstddef>
#include <cstdint>

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

    const detail::Backend *backend_;
    Configuration configuration_{};
    Frame *active_frame_ = nullptr;
    bool initialized_ = false;
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
