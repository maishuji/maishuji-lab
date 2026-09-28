#pragma once

#include <cmath>

namespace maishuji {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2 operator+(const Vec2 &other) const noexcept {
        return {x + other.x, y + other.y};
    }

    constexpr Vec2 operator-(const Vec2 &other) const noexcept {
        return {x - other.x, y - other.y};
    }

    constexpr Vec2 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar};
    }
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vec3 operator+(const Vec3 &other) const noexcept {
        return {x + other.x, y + other.y, z + other.z};
    }

    constexpr Vec3 operator-(const Vec3 &other) const noexcept {
        return {x - other.x, y - other.y, z - other.z};
    }

    constexpr Vec3 operator-() const noexcept {
        return {-x, -y, -z};
    }

    constexpr Vec3 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar, z * scalar};
    }
};

constexpr float dot(Vec3 first, Vec3 second) noexcept {
    return first.x * second.x + first.y * second.y + first.z * second.z;
}

constexpr Vec3 cross(Vec3 first, Vec3 second) noexcept {
    return {
        first.y * second.z - first.z * second.y,
        first.z * second.x - first.x * second.z,
        first.x * second.y - first.y * second.x,
    };
}

inline Vec3 normalize(Vec3 value) noexcept {
    const float length = std::sqrt(dot(value, value));
    if(length <= 0.000001f)
        return {};
    return value * (1.0f / length);
}

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    constexpr Vec4 operator+(const Vec4 &other) const noexcept {
        return {x + other.x, y + other.y, z + other.z, w + other.w};
    }

    constexpr Vec4 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar, z * scalar, w * scalar};
    }
};

struct Mat4 {
    float values[4][4]{};

    static constexpr Mat4 identity() noexcept {
        Mat4 result{};
        result.values[0][0] = 1.0f;
        result.values[1][1] = 1.0f;
        result.values[2][2] = 1.0f;
        result.values[3][3] = 1.0f;
        return result;
    }
};

constexpr Vec4 operator*(const Mat4 &matrix, Vec4 vector) noexcept {
    return {
        matrix.values[0][0] * vector.x +
            matrix.values[0][1] * vector.y +
            matrix.values[0][2] * vector.z +
            matrix.values[0][3] * vector.w,
        matrix.values[1][0] * vector.x +
            matrix.values[1][1] * vector.y +
            matrix.values[1][2] * vector.z +
            matrix.values[1][3] * vector.w,
        matrix.values[2][0] * vector.x +
            matrix.values[2][1] * vector.y +
            matrix.values[2][2] * vector.z +
            matrix.values[2][3] * vector.w,
        matrix.values[3][0] * vector.x +
            matrix.values[3][1] * vector.y +
            matrix.values[3][2] * vector.z +
            matrix.values[3][3] * vector.w,
    };
}

constexpr Mat4 operator*(const Mat4 &left, const Mat4 &right) noexcept {
    Mat4 result{};
    for(int row = 0; row < 4; ++row) {
        for(int column = 0; column < 4; ++column) {
            for(int index = 0; index < 4; ++index) {
                result.values[row][column] +=
                    left.values[row][index] * right.values[index][column];
            }
        }
    }
    return result;
}

constexpr Vec4 to_vec4(Vec3 value, float w = 1.0f) noexcept {
    return {value.x, value.y, value.z, w};
}

constexpr Vec3 to_vec3(Vec4 value) noexcept {
    return {value.x, value.y, value.z};
}

constexpr Mat4 make_translation(Vec3 translation) noexcept {
    Mat4 result = Mat4::identity();
    result.values[0][3] = translation.x;
    result.values[1][3] = translation.y;
    result.values[2][3] = translation.z;
    return result;
}

constexpr Mat4 make_scale(Vec3 scale) noexcept {
    Mat4 result{};
    result.values[0][0] = scale.x;
    result.values[1][1] = scale.y;
    result.values[2][2] = scale.z;
    result.values[3][3] = 1.0f;
    return result;
}

inline Mat4 make_rotation_x(float radians) noexcept {
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    Mat4 result = Mat4::identity();
    result.values[1][1] = cosine;
    result.values[1][2] = -sine;
    result.values[2][1] = sine;
    result.values[2][2] = cosine;
    return result;
}

inline Mat4 make_rotation_y(float radians) noexcept {
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    Mat4 result = Mat4::identity();
    result.values[0][0] = cosine;
    result.values[0][2] = sine;
    result.values[2][0] = -sine;
    result.values[2][2] = cosine;
    return result;
}

inline Mat4 make_rotation_z(float radians) noexcept {
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    Mat4 result = Mat4::identity();
    result.values[0][0] = cosine;
    result.values[0][1] = -sine;
    result.values[1][0] = sine;
    result.values[1][1] = cosine;
    return result;
}

struct Transform {
    Vec3 position{};
    Vec3 rotation_radians{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

inline Mat4 transform_matrix(const Transform &transform) noexcept {
    const Mat4 rotation =
        make_rotation_z(transform.rotation_radians.z) *
        make_rotation_y(transform.rotation_radians.y) *
        make_rotation_x(transform.rotation_radians.x);
    return make_translation(transform.position) * rotation *
           make_scale(transform.scale);
}

struct Camera {
    Vec3 position{0.0f, 0.0f, 3.0f};
    Vec3 target{};
    Vec3 up{0.0f, 1.0f, 0.0f};
    float vertical_fov_radians = 1.04719755f;
    float aspect_ratio = 4.0f / 3.0f;
    float near_plane = 0.1f;
    float far_plane = 100.0f;

    inline bool valid() const noexcept {
        constexpr float pi = 3.14159265f;
        const Vec3 forward = target - position;
        const Vec3 side = cross(forward, up);
        const float forward_length = dot(forward, forward);
        const float up_length = dot(up, up);
        const float side_length = dot(side, side);

        return std::isfinite(position.x) && std::isfinite(position.y) &&
               std::isfinite(position.z) && std::isfinite(target.x) &&
               std::isfinite(target.y) && std::isfinite(target.z) &&
               std::isfinite(up.x) && std::isfinite(up.y) &&
               std::isfinite(up.z) &&
               forward_length > 0.000001f && up_length > 0.000001f &&
               side_length > 0.000001f &&
               std::isfinite(vertical_fov_radians) &&
               vertical_fov_radians > 0.0f && vertical_fov_radians < pi &&
               std::isfinite(aspect_ratio) && aspect_ratio > 0.0f &&
               std::isfinite(near_plane) && near_plane > 0.0f &&
               std::isfinite(far_plane) && far_plane > near_plane;
    }
};

inline Mat4 view_matrix(const Camera &camera) noexcept {
    if(!camera.valid())
        return Mat4::identity();

    const Vec3 forward = normalize(camera.target - camera.position);
    const Vec3 right = normalize(cross(forward, camera.up));
    const Vec3 corrected_up = cross(right, forward);

    Mat4 result = Mat4::identity();
    result.values[0][0] = right.x;
    result.values[0][1] = right.y;
    result.values[0][2] = right.z;
    result.values[0][3] = -dot(right, camera.position);
    result.values[1][0] = corrected_up.x;
    result.values[1][1] = corrected_up.y;
    result.values[1][2] = corrected_up.z;
    result.values[1][3] = -dot(corrected_up, camera.position);
    result.values[2][0] = -forward.x;
    result.values[2][1] = -forward.y;
    result.values[2][2] = -forward.z;
    result.values[2][3] = dot(forward, camera.position);
    return result;
}

inline Mat4 projection_matrix(const Camera &camera) noexcept {
    if(!camera.valid())
        return Mat4::identity();

    const float tangent = std::tan(camera.vertical_fov_radians * 0.5f);
    const float focal_length = 1.0f / tangent;
    const float depth_range = camera.near_plane - camera.far_plane;

    Mat4 result{};
    result.values[0][0] = focal_length / camera.aspect_ratio;
    result.values[1][1] = focal_length;
    result.values[2][2] =
        (camera.far_plane + camera.near_plane) / depth_range;
    result.values[2][3] =
        (2.0f * camera.far_plane * camera.near_plane) / depth_range;
    result.values[3][2] = -1.0f;
    return result;
}

inline Mat4 view_projection_matrix(const Camera &camera) noexcept {
    if(!camera.valid())
        return Mat4::identity();
    return projection_matrix(camera) * view_matrix(camera);
}

struct ProjectedPoint {
    Vec3 normalized_device{};
    float clip_w = 0.0f;
    bool valid = false;
};

inline ProjectedPoint project_point(const Mat4 &transform,
                                    Vec3 position) noexcept {
    const Vec4 clip = transform * to_vec4(position);
    if(clip.w <= 0.000001f)
        return {{}, clip.w, false};

    return {
        {clip.x / clip.w, clip.y / clip.w, clip.z / clip.w},
        clip.w,
        true,
    };
}

} // namespace maishuji
