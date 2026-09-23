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
    }

    return "unknown status";
}

Pvr::Pvr() noexcept
    : backend_(&detail::default_backend()) {}

Pvr::~Pvr() noexcept {
    assert(!initialized_);
    assert(active_frame_ == nullptr);
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
