#include "maishuji/pvr.hpp"

#include "detail/backend.hpp"

#include <cassert>
#include <limits>

namespace maishuji {
namespace {

bool valid_configuration(const Configuration &configuration) noexcept {
    const bool has_enabled_list =
        configuration.enable_opaque ||
        configuration.enable_punch_through ||
        configuration.enable_translucent;

    return has_enabled_list &&
           configuration.vertex_buffer_bytes >= 32 &&
           configuration.vertex_buffer_bytes % 32 == 0 &&
           configuration.vertex_buffer_bytes <=
               static_cast<std::size_t>(std::numeric_limits<int>::max());
}

std::uint8_t list_bit(List list) noexcept {
    switch(list) {
    case List::Opaque:
        return 1u << 0;
    case List::PunchThrough:
        return 1u << 1;
    case List::Translucent:
        return 1u << 2;
    }

    return 0;
}

bool list_enabled(const Configuration &configuration, List list) noexcept {
    switch(list) {
    case List::Opaque:
        return configuration.enable_opaque;
    case List::PunchThrough:
        return configuration.enable_punch_through;
    case List::Translucent:
        return configuration.enable_translucent;
    }

    return false;
}

bool power_of_two(std::uint16_t value) noexcept {
    return value >= 4 && value <= 1024 &&
           (value & static_cast<std::uint16_t>(value - 1)) == 0;
}

bool valid_texture_dimensions(std::uint16_t width,
                              std::uint16_t height) noexcept {
    return power_of_two(width) && power_of_two(height);
}

std::size_t texture_bytes(std::uint16_t width,
                          std::uint16_t height) noexcept {
    return static_cast<std::size_t>(width) *
           static_cast<std::size_t>(height) * sizeof(std::uint16_t);
}

} // namespace

const char *status_name(Status status) noexcept {
    switch(status) {
    case Status::Success:
        return "success";
    case Status::AlreadyInitialized:
        return "already initialized";
    case Status::NotInitialized:
        return "not initialized";
    case Status::InvalidConfiguration:
        return "invalid configuration";
    case Status::BackendInitializationFailed:
        return "backend initialization failed";
    case Status::BackendShutdownFailed:
        return "backend shutdown failed";
    case Status::FrameAlreadyActive:
        return "frame already active";
    case Status::FrameActive:
        return "frame still active";
    case Status::FrameNotActive:
        return "frame not active";
    case Status::WaitReadyFailed:
        return "PVR ready wait failed";
    case Status::SceneBeginFailed:
        return "PVR scene begin failed";
    case Status::SceneFinishFailed:
        return "PVR scene finish failed";
    case Status::RenderWaitFailed:
        return "PVR render wait failed";
    case Status::RenderListAlreadyActive:
        return "render list already active";
    case Status::RenderListAlreadyFinished:
        return "render list already finished in this frame";
    case Status::RenderListActive:
        return "render list still active";
    case Status::RenderListNotActive:
        return "render list not active";
    case Status::RenderListDisabled:
        return "render list disabled";
    case Status::RenderListBeginFailed:
        return "PVR render-list begin failed";
    case Status::RenderListFinishFailed:
        return "PVR render-list finish failed";
    case Status::PrimitiveSubmissionFailed:
        return "PVR primitive submission failed";
    case Status::TextureAlreadyAllocated:
        return "texture already allocated";
    case Status::TextureNotAllocated:
        return "texture not allocated";
    case Status::TextureInvalidDimensions:
        return "texture dimensions must be power-of-two values from 4 to 1024";
    case Status::TextureAllocationFailed:
        return "texture VRAM allocation failed";
    case Status::TextureInvalidData:
        return "texture upload data has the wrong size";
    case Status::TextureUploadFailed:
        return "texture upload failed";
    case Status::TextureContextMismatch:
        return "texture belongs to a different PVR context";
    }

    return "unknown status";
}

Pvr::Pvr() noexcept
    : backend_(&detail::default_backend()) {}

Pvr::~Pvr() noexcept {
    assert(!initialized_);
    assert(active_frame_ == nullptr);
}

Texture::~Texture() noexcept {
    assert(!allocated_);
}

Texture::Texture(Texture &&other) noexcept
    : owner_(other.owner_),
      handle_(other.handle_),
      width_(other.width_),
      height_(other.height_),
      allocated_(other.allocated_) {
    other.owner_ = nullptr;
    other.handle_ = 0;
    other.width_ = 0;
    other.height_ = 0;
    other.allocated_ = false;
}

Texture &Texture::operator=(Texture &&other) noexcept {
    if(this == &other)
        return *this;

    assert(!allocated_);
    owner_ = other.owner_;
    handle_ = other.handle_;
    width_ = other.width_;
    height_ = other.height_;
    allocated_ = other.allocated_;

    other.owner_ = nullptr;
    other.handle_ = 0;
    other.width_ = 0;
    other.height_ = 0;
    other.allocated_ = false;
    return *this;
}

Status Texture::allocate(Pvr &pvr, std::uint16_t width,
                         std::uint16_t height) noexcept {
    if(allocated_)
        return Status::TextureAlreadyAllocated;
    if(!pvr.initialized_)
        return Status::NotInitialized;
    if(!valid_texture_dimensions(width, height))
        return Status::TextureInvalidDimensions;

    detail::TextureHandle handle = 0;
    if(!pvr.backend_->texture_allocate(texture_bytes(width, height), handle))
        return Status::TextureAllocationFailed;

    owner_ = &pvr;
    handle_ = handle;
    width_ = width;
    height_ = height;
    allocated_ = true;
    return Status::Success;
}

Status Texture::upload(std::span<const std::uint16_t> pixels) noexcept {
    if(!allocated_ || owner_ == nullptr)
        return Status::TextureNotAllocated;

    const std::size_t expected_pixels =
        static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
    if(pixels.size() != expected_pixels)
        return Status::TextureInvalidData;

    if(!owner_->backend_->texture_upload(
           static_cast<detail::TextureHandle>(handle_), pixels.data(),
           texture_bytes(width_, height_)))
        return Status::TextureUploadFailed;

    return Status::Success;
}

Status Texture::release() noexcept {
    if(!allocated_ || owner_ == nullptr)
        return Status::TextureNotAllocated;
    if(!owner_->initialized_)
        return Status::NotInitialized;

    const Status wait_status = owner_->wait_render_done();
    if(failed(wait_status))
        return wait_status;

    owner_->backend_->texture_free(
        static_cast<detail::TextureHandle>(handle_));
    owner_ = nullptr;
    handle_ = 0;
    width_ = 0;
    height_ = 0;
    allocated_ = false;
    return Status::Success;
}

Status Pvr::initialize(const Configuration &configuration) noexcept {
    if(initialized_)
        return Status::AlreadyInitialized;
    if(!valid_configuration(configuration))
        return Status::InvalidConfiguration;
    if(!backend_->initialize(configuration))
        return Status::BackendInitializationFailed;

    configuration_ = configuration;
    initialized_ = true;
    return Status::Success;
}

Status Pvr::begin_frame(Frame &frame) noexcept {
    if(!initialized_)
        return Status::NotInitialized;
    if(active_frame_ != nullptr || frame.active_)
        return Status::FrameAlreadyActive;
    if(!backend_->wait_ready())
        return Status::WaitReadyFailed;
    if(!backend_->scene_begin())
        return Status::SceneBeginFailed;

    frame.owner_ = this;
    frame.active_ = true;
    frame.active_list_ = nullptr;
    frame.started_lists_ = 0;
    frame.last_status_ = Status::Success;
    active_frame_ = &frame;
    return Status::Success;
}

Status Pvr::wait_render_done() noexcept {
    if(!initialized_)
        return Status::NotInitialized;
    if(active_frame_ != nullptr)
        return Status::FrameActive;
    return backend_->wait_render_done()
               ? Status::Success
               : Status::RenderWaitFailed;
}

Status Pvr::shutdown() noexcept {
    if(!initialized_)
        return Status::NotInitialized;
    if(active_frame_ != nullptr)
        return Status::FrameActive;

    const bool render_waited = backend_->wait_render_done();
    const bool shutdown_succeeded = backend_->shutdown();

    initialized_ = false;
    configuration_ = {};
    if(!render_waited)
        return Status::RenderWaitFailed;
    if(!shutdown_succeeded)
        return Status::BackendShutdownFailed;
    return Status::Success;
}

Frame::~Frame() noexcept {
    if(active_)
        (void)close_from_destructor();
}

Status Frame::begin_list(RenderList &list, List list_type) noexcept {
    if(!active_ || owner_ == nullptr)
        return Status::FrameNotActive;
    if(active_list_ != nullptr || list.active_)
        return Status::RenderListAlreadyActive;
    if((started_lists_ & list_bit(list_type)) != 0)
        return Status::RenderListAlreadyFinished;
    if(!list_enabled(owner_->configuration_, list_type))
        return Status::RenderListDisabled;
    if(!owner_->backend_->list_begin(list_type))
        return Status::RenderListBeginFailed;

    list.owner_ = this;
    list.list_type_ = list_type;
    list.active_ = true;
    list.last_status_ = Status::Success;
    started_lists_ |= list_bit(list_type);
    active_list_ = &list;
    return Status::Success;
}

Status Frame::finish() noexcept {
    if(!active_ || owner_ == nullptr)
        return Status::FrameNotActive;
    if(active_list_ != nullptr)
        return Status::RenderListActive;

    const Status status = owner_->backend_->scene_finish()
                              ? Status::Success
                              : Status::SceneFinishFailed;
    Pvr *pvr = owner_;
    owner_ = nullptr;
    active_ = false;
    pvr->active_frame_ = nullptr;
    last_status_ = status;
    return status;
}

Status Frame::close_from_destructor() noexcept {
    Status list_status = Status::Success;
    if(active_list_ != nullptr)
        list_status = active_list_->finish();

    const Status frame_status = finish();
    if(frame_status == Status::Success && list_status != Status::Success)
        last_status_ = list_status;
    return frame_status == Status::Success ? list_status : frame_status;
}

RenderList::~RenderList() noexcept {
    if(active_)
        (void)finish();
}

Status RenderList::submit(
    const Triangle &triangle,
    const PrimitiveConfiguration &configuration) noexcept {
    if(!active_ || owner_ == nullptr || owner_->owner_ == nullptr ||
       owner_->active_list_ != this)
        return Status::RenderListNotActive;

    return owner_->owner_->backend_->submit_triangle(
               list_type_, triangle, configuration)
               ? Status::Success
               : Status::PrimitiveSubmissionFailed;
}

Status RenderList::submit(
    const Quad &quad,
    const PrimitiveConfiguration &configuration) noexcept {
    if(!active_ || owner_ == nullptr || owner_->owner_ == nullptr ||
       owner_->active_list_ != this)
        return Status::RenderListNotActive;

    return owner_->owner_->backend_->submit_quad(
               list_type_, quad, configuration)
               ? Status::Success
               : Status::PrimitiveSubmissionFailed;
}

Status RenderList::submit(
    const Texture &texture,
    const TexturedQuad &quad,
    const PrimitiveConfiguration &configuration) noexcept {
    if(!active_ || owner_ == nullptr || owner_->owner_ == nullptr ||
       owner_->active_list_ != this)
        return Status::RenderListNotActive;
    if(!texture.allocated_ || texture.owner_ == nullptr)
        return Status::TextureNotAllocated;
    if(texture.owner_ != owner_->owner_)
        return Status::TextureContextMismatch;

    return owner_->owner_->backend_->submit_textured_quad(
               list_type_,
               static_cast<detail::TextureHandle>(texture.handle_),
               texture.width_, texture.height_, quad, configuration)
               ? Status::Success
               : Status::PrimitiveSubmissionFailed;
}

Status RenderList::finish() noexcept {
    if(!active_ || owner_ == nullptr)
        return Status::RenderListNotActive;

    Frame *frame = owner_;
    const Status status = frame->owner_->backend_->list_finish()
                              ? Status::Success
                              : Status::RenderListFinishFailed;
    owner_ = nullptr;
    active_ = false;
    if(frame->active_list_ == this)
        frame->active_list_ = nullptr;
    last_status_ = status;
    return status;
}

} // namespace maishuji
