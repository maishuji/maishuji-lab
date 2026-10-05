#pragma once

#include "maishuji/math.hpp"

#include <algorithm>
#include <cmath>

namespace maishuji {

struct BoundingSphere {
    Vec3 center{};
    float radius = 0.0f;
};

enum class FrustumVisibility {
    Outside,
    Intersects,
    Inside,
};

// A conservative object-level test. An invalid input stays visible so that
// this optimization never silently drops geometry.
inline FrustumVisibility classify_sphere(
    const Camera &camera, const Transform &transform,
    BoundingSphere local_sphere) noexcept {
    if(!camera.valid() || !std::isfinite(local_sphere.center.x) ||
       !std::isfinite(local_sphere.center.y) ||
       !std::isfinite(local_sphere.center.z) ||
       !std::isfinite(local_sphere.radius) || local_sphere.radius < 0.0f ||
       !std::isfinite(transform.scale.x) ||
       !std::isfinite(transform.scale.y) ||
       !std::isfinite(transform.scale.z))
        return FrustumVisibility::Intersects;

    const Vec4 world = transform_matrix(transform) * to_vec4(local_sphere.center);
    const Vec4 view = view_matrix(camera) * world;
    const float scale = std::max({std::fabs(transform.scale.x),
                                  std::fabs(transform.scale.y),
                                  std::fabs(transform.scale.z)});
    const float radius = local_sphere.radius * scale;
    if(!std::isfinite(view.x) || !std::isfinite(view.y) ||
       !std::isfinite(view.z) || !std::isfinite(radius))
        return FrustumVisibility::Intersects;

    const float depth = -view.z;
    const float vertical = std::tan(camera.vertical_fov_radians * 0.5f);
    const float horizontal = vertical * camera.aspect_ratio;
    const float horizontal_length = std::sqrt(1.0f + horizontal * horizontal);
    const float vertical_length = std::sqrt(1.0f + vertical * vertical);
    const float distances[] = {
        depth - camera.near_plane,
        camera.far_plane - depth,
        (depth * horizontal + view.x) / horizontal_length,
        (depth * horizontal - view.x) / horizontal_length,
        (depth * vertical + view.y) / vertical_length,
        (depth * vertical - view.y) / vertical_length,
    };

    for(float distance : distances) {
        if(!std::isfinite(distance))
            return FrustumVisibility::Intersects;
    }
    FrustumVisibility result = FrustumVisibility::Inside;
    for(float distance : distances) {
        if(distance < -radius)
            return FrustumVisibility::Outside;
        if(distance < radius)
            result = FrustumVisibility::Intersects;
    }
    return result;
}

} // namespace maishuji
