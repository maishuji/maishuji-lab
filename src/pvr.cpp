#include "maishuji/pvr.hpp"
#include "maishuji/mesh.hpp"

#include "detail/backend.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
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

struct ClipVertex {
    Vec4 position{};
    Color color{};
    Color offset_color{0, 0, 0, 0};
};

constexpr float clip_epsilon = 0.000001f;

float clip_distance(const ClipVertex &vertex, int plane) noexcept {
    switch(plane) {
    case 0:
        return vertex.position.x + vertex.position.w;
    case 1:
        return vertex.position.w - vertex.position.x;
    case 2:
        return vertex.position.y + vertex.position.w;
    case 3:
        return vertex.position.w - vertex.position.y;
    case 4:
        return vertex.position.z + vertex.position.w;
    case 5:
        return vertex.position.w - vertex.position.z;
    }

    return -1.0f;
}

std::uint8_t interpolate_channel(std::uint8_t first, std::uint8_t second,
                                  float amount) noexcept {
    const float value =
        static_cast<float>(first) +
        (static_cast<float>(second) - static_cast<float>(first)) * amount;
    return static_cast<std::uint8_t>(
        std::clamp(value, 0.0f, 255.0f));
}

ClipVertex interpolate_clip_vertex(const ClipVertex &first,
                                   const ClipVertex &second,
                                   float amount) noexcept {
    return {
        {
            first.position.x +
                (second.position.x - first.position.x) * amount,
            first.position.y +
                (second.position.y - first.position.y) * amount,
            first.position.z +
                (second.position.z - first.position.z) * amount,
            first.position.w +
                (second.position.w - first.position.w) * amount,
        },
        {
            interpolate_channel(first.color.red, second.color.red, amount),
            interpolate_channel(first.color.green, second.color.green, amount),
            interpolate_channel(first.color.blue, second.color.blue, amount),
            interpolate_channel(first.color.alpha, second.color.alpha, amount),
        },
        {
            interpolate_channel(first.offset_color.red, second.offset_color.red, amount),
            interpolate_channel(first.offset_color.green, second.offset_color.green, amount),
            interpolate_channel(first.offset_color.blue, second.offset_color.blue, amount),
            interpolate_channel(first.offset_color.alpha, second.offset_color.alpha, amount),
        },
    };
}

bool clip_triangle(const ClipVertex input[3],
                   std::array<ClipVertex, 12> &polygon,
                   std::size_t &polygon_count) noexcept {
    std::array<ClipVertex, 12> current{};
    std::array<ClipVertex, 12> next{};
    current[0] = input[0];
    current[1] = input[1];
    current[2] = input[2];
    polygon_count = 3;

    for(int plane = 0; plane < 6; ++plane) {
        std::size_t next_count = 0;
        ClipVertex previous = current[polygon_count - 1];
        float previous_distance = clip_distance(previous, plane);
        bool previous_inside = previous_distance >= 0.0f;

        for(std::size_t index = 0; index < polygon_count; ++index) {
            const ClipVertex current_vertex = current[index];
            const float current_distance =
                clip_distance(current_vertex, plane);
            const bool current_inside = current_distance >= 0.0f;

            if(current_inside != previous_inside) {
                const float denominator = previous_distance - current_distance;
                if(std::fabs(denominator) <= clip_epsilon ||
                   next_count >= next.size())
                    return false;

                next[next_count++] = interpolate_clip_vertex(
                    previous, current_vertex,
                    previous_distance / denominator);
            }
            if(current_inside) {
                if(next_count >= next.size())
                    return false;
                next[next_count++] = current_vertex;
            }

            previous = current_vertex;
            previous_distance = current_distance;
            previous_inside = current_inside;
        }

        current = next;
        polygon_count = next_count;
        if(polygon_count == 0)
            break;
    }

    polygon = current;
    return true;
}

bool to_screen_vertex(const ClipVertex &source, const Viewport &viewport,
                      Vertex &destination) noexcept {
    if(source.position.w <= clip_epsilon)
        return false;

    const float inverse_w = 1.0f / source.position.w;
    const float normalized_x = source.position.x * inverse_w;
    const float normalized_y = source.position.y * inverse_w;
    const float normalized_z = source.position.z * inverse_w;
    if(!std::isfinite(normalized_x) || !std::isfinite(normalized_y) ||
       !std::isfinite(normalized_z))
        return false;

    destination = {
        (normalized_x + 1.0f) * 0.5f * viewport.width,
        (1.0f - normalized_y) * 0.5f * viewport.height,
        1.0f - (normalized_z + 1.0f) * 0.5f,
        source.color,
        source.offset_color,
    };
    return true;
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
    case Status::TextureActive:
        return "texture still allocated";
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
    case Status::MeshInvalidData:
        return "mesh data or viewport is invalid";
    case Status::InvalidCamera:
        return "camera parameters are invalid";
    case Status::InvalidFog:
        return "fog parameters are invalid";
    case Status::MeshProjectionFailed:
        return "mesh vertex cannot be projected";
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
    ++pvr.active_texture_count_;
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
    assert(owner_->active_texture_count_ > 0);
    --owner_->active_texture_count_;
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
    if(active_texture_count_ != 0)
        return Status::TextureActive;

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

Status RenderList::submit(
    const Mesh &mesh, const Camera &camera, const Transform &transform,
    const Viewport &viewport,
    const PrimitiveConfiguration &configuration) noexcept {
    return submit_fogged(mesh, camera, transform, viewport, Fog{},
                         configuration);
}

Status RenderList::submit_fogged(
    const Mesh &mesh, const Camera &camera, const Transform &transform,
    const Viewport &viewport, const Fog &fog,
    const PrimitiveConfiguration &configuration) noexcept {
    if(!active_ || owner_ == nullptr || owner_->owner_ == nullptr ||
       owner_->active_list_ != this)
        return Status::RenderListNotActive;
    if(mesh.vertices.empty() || mesh.indices.empty() ||
       mesh.indices.size() % 3 != 0 ||
       !std::isfinite(viewport.width) || !std::isfinite(viewport.height) ||
       viewport.width <= 0.0f || viewport.height <= 0.0f)
        return Status::MeshInvalidData;
    if(!camera.valid())
        return Status::InvalidCamera;
    if(!fog.valid())
        return Status::InvalidFog;

    for(const std::uint16_t index : mesh.indices) {
        if(index >= mesh.vertices.size())
            return Status::MeshInvalidData;
    }

    const Mat4 model = transform_matrix(transform);
    const Mat4 view = view_matrix(camera);
    const Mat4 model_view = view * model;
    const Mat4 model_view_projection = projection_matrix(camera) * model_view;

    for(std::size_t index = 0; index < mesh.indices.size(); index += 3) {
        const MeshVertex *source_vertices[] = {
            &mesh.vertices[mesh.indices[index]],
            &mesh.vertices[mesh.indices[index + 1]],
            &mesh.vertices[mesh.indices[index + 2]],
        };
        const Vec4 view_positions[3] = {
            model_view * to_vec4(source_vertices[0]->position),
            model_view * to_vec4(source_vertices[1]->position),
            model_view * to_vec4(source_vertices[2]->position),
        };
        const ClipVertex input[3] = {
            {model_view_projection * to_vec4(source_vertices[0]->position),
             fog.apply(source_vertices[0]->color, -view_positions[0].z),
             source_vertices[0]->offset_color},
            {model_view_projection * to_vec4(source_vertices[1]->position),
             fog.apply(source_vertices[1]->color, -view_positions[1].z),
             source_vertices[1]->offset_color},
            {model_view_projection * to_vec4(source_vertices[2]->position),
             fog.apply(source_vertices[2]->color, -view_positions[2].z),
             source_vertices[2]->offset_color},
        };

        std::array<ClipVertex, 12> polygon{};
        std::size_t polygon_count = 0;
        if(!clip_triangle(input, polygon, polygon_count))
            return Status::MeshProjectionFailed;
        if(polygon_count < 3)
            continue;

        for(std::size_t vertex_index = 1; vertex_index + 1 < polygon_count;
            ++vertex_index) {
            Vertex projected[3]{};
            if(!to_screen_vertex(polygon[0], viewport, projected[0]) ||
               !to_screen_vertex(polygon[vertex_index], viewport,
                                 projected[1]) ||
               !to_screen_vertex(polygon[vertex_index + 1], viewport,
                                 projected[2]))
                return Status::MeshProjectionFailed;

            const Triangle triangle{
                projected[0],
                projected[1],
                projected[2],
            };
            if(!owner_->owner_->backend_->submit_triangle(
                   list_type_, triangle, configuration))
                return Status::PrimitiveSubmissionFailed;
        }
    }

    return Status::Success;
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
