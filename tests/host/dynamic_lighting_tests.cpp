#include "../../advanced_examples/dynamic_lighting.hpp"
#include "car_2cv_mesh.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <tuple>

namespace {

namespace lighting = dynamic_lighting;
using maishuji::Vec3;
int failures = 0;

void expect(bool condition, const char *message) {
    if(condition) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

bool near(float actual, float expected, float tolerance = 1.0e-5f) {
    return std::isfinite(actual) && std::isfinite(expected) &&
           std::fabs(actual - expected) <= tolerance;
}

bool same(Vec3 a, Vec3 b, float tolerance = 1.0e-5f) {
    return near(a.x, b.x, tolerance) && near(a.y, b.y, tolerance) &&
           near(a.z, b.z, tolerance);
}

bool same(const maishuji::TexturedMeshVertex &a,
          const maishuji::TexturedMeshVertex &b) {
    return same(a.position, b.position, 0.0f) && a.u == b.u && a.v == b.v &&
           a.color.argb() == b.color.argb() &&
           a.offset_color.argb() == b.offset_color.argb();
}

void test_facets_and_lambert() {
    lighting::Facet facet{};
    expect(lighting::make_facet({0, 0, 0}, {0, 0, 2}, {2, 0, 0}, facet),
           "nondegenerate triangle has a facet");
    expect(same(facet.normal, {0, 1, 0}), "cross(b-a,c-a) gives upward normal");
    expect(near(maishuji::dot(facet.normal, facet.normal), 1), "normal is unit length");
    expect(same(facet.centroid, {2.0f / 3.0f, 0, 2.0f / 3.0f}),
           "centroid is the mean of all three positions");
    expect(near(lighting::lambert(facet, facet.centroid + Vec3{0, 5, 0}), 1),
           "front light reaches ambient plus diffuse");
    expect(near(lighting::lambert(facet, facet.centroid + Vec3{0, -5, 0}), 0.30f),
           "back light clamps diffuse, preserving ambient");
    expect(near(lighting::lambert(facet, facet.centroid + Vec3{5, 0, 0}), 0.30f),
           "tangent light receives ambient only");
    expect(near(lighting::lambert(facet, facet.centroid + Vec3{0, 1, 1}),
                0.30f + 0.70f / std::sqrt(2.0f)), "Lambert follows cosine angle");
    expect(near(lighting::lambert(facet, facet.centroid), 0.30f),
           "coincident light has a finite ambient fallback");
    expect(near(lighting::lambert(facet, {0, std::numeric_limits<float>::infinity(), 0}),
                0.30f), "invalid light does not create invalid colors");

    lighting::Facet reversed{};
    expect(lighting::make_facet({0, 0, 0}, {2, 0, 0}, {0, 0, 2}, reversed),
           "reversed triangle remains valid");
    expect(same(reversed.normal, {0, -1, 0}), "winding reverses the normal");
    const auto saved = facet;
    expect(!lighting::make_facet({}, {1, 0, 0}, {2, 0, 0}, facet),
           "collinear triangle is rejected");
    expect(same(saved.normal, facet.normal) && same(saved.centroid, facet.centroid),
           "failed facet computation leaves the output untouched");
    expect(!lighting::make_facet({}, {0, 1, 0},
                                 {std::numeric_limits<float>::quiet_NaN(), 0, 0}, facet),
           "nonfinite triangle is rejected");
    const maishuji::Color base{100, 200, 250, 137};
    const auto shaded = lighting::modulate(base, 0.30f);
    expect(shaded.red == 30 && shaded.green == 60 && shaded.blue == 75 &&
           shaded.alpha == 137, "lighting modulates RGB, preserving base alpha");
    expect(lighting::modulate(base, 1).argb() == base.argb(), "full light preserves base color");
}

void test_projection() {
    const Vec3 point{0.4f, 1.6f, -0.7f};
    const Vec3 light{4, 4.5f, 0};
    Vec3 projected{};
    expect(lighting::project_to_ground(point, light, projected), "point projects onto ground");
    expect(projected.y == 0.0f, "mathematical receiver is exactly y=0, without render bias");
    const float t = point.y / (light.y - point.y);
    expect(same(projected, point + (point - light) * t), "projection follows specified ray formula");
    expect(same(maishuji::cross(projected - light, point - light), {}, 1.0e-4f),
           "light, model vertex and shadow lie on one ray");
    expect(maishuji::dot(projected - point, point - light) > 0,
           "shadow extends away from the point light");
    Vec3 positive{}, negative{};
    expect(lighting::project_to_ground({0, 1.6f, 0}, light, negative),
           "+X light projects elevated point");
    expect(lighting::project_to_ground({0, 1.6f, 0}, {-4, 4.5f, 0}, positive),
           "-X light projects elevated point");
    expect(negative.x < -2.0f && positive.x > 2.0f && near(negative.x, -positive.x),
           "opposite lights produce visibly opposite ground displacement");
    const Vec3 contact{0.7f, 0, 1.1f};
    expect(lighting::project_to_ground(contact, light, projected) && same(projected, contact),
           "ground contact point stays fixed");

    const Vec3 sentinel{7, 8, 9};
    const auto reject = [&](Vec3 p, Vec3 l, const char *message) {
        Vec3 output = sentinel;
        expect(!lighting::project_to_ground(p, l, output), message);
        expect(same(output, sentinel), "rejected projection does not overwrite output");
    };
    reject(point, {0, 1.6f, 0}, "light at vertex height is singular");
    reject(point, {0, 1.0f, 0}, "light below vertex is rejected");
    reject(point, {0, 1.600001f, 0}, "near-singular denominator is rejected");
    reject({0, -0.1f, 0}, light, "vertices below receiver are rejected");
    reject({}, {0, 0, 0}, "light on receiver is rejected even for contact vertices");
    const float invalid_values[] = {std::numeric_limits<float>::quiet_NaN(),
                                    std::numeric_limits<float>::infinity(),
                                    -std::numeric_limits<float>::infinity()};
    for(float invalid : invalid_values) {
        reject({invalid, point.y, point.z}, light, "nonfinite point x is rejected");
        reject({point.x, invalid, point.z}, light, "nonfinite point y is rejected");
        reject({point.x, point.y, invalid}, light, "nonfinite point z is rejected");
        reject(point, {invalid, light.y, light.z}, "nonfinite light x is rejected");
        reject(point, {light.x, invalid, light.z}, "nonfinite light y is rejected");
        reject(point, {light.x, light.y, invalid}, "nonfinite light z is rejected");
    }
    reject({std::numeric_limits<float>::max(), 1, 0},
           {-std::numeric_limits<float>::max(), 2, 0}, "overflowing finite-input ray is rejected");
}

void test_shared_vertex_expansion() {
    constexpr std::array<maishuji::TexturedMeshVertex, 4> source{{
        {{0, 0, 0}, 0.1f, 0.2f, {200, 150, 100, 255}},
        {{0, 0, 1}, 0.3f, 0.4f, {200, 150, 100, 255}},
        {{1, 0, 0}, 0.5f, 0.6f, {200, 150, 100, 255}},
        {{0, 1, 0}, 0.7f, 0.8f, {200, 150, 100, 255}},
    }};
    constexpr std::array<std::uint16_t, 6> indices{0, 1, 2, 0, 2, 3};
    static lighting::FlatCar<6> flat;
    expect(!flat.update({0, 4.5f, -4}), "uninitialized storage cannot be updated");
    expect(flat.initialize(source, indices), "shared-vertex mesh expands");
    expect(same(flat.base[0], flat.base[3]), "shared vertex is copied into both facets");
    expect(flat.indices[0] == 0 && flat.indices[3] == 3,
           "separate triangles reference separate vertices");
    expect(same(flat.facets[0].normal, {0, 1, 0}) &&
           same(flat.facets[1].normal, {0, 0, 1}), "each triangle has its own normal");
    expect(flat.update({0, 4.5f, -4}), "expanded mesh lights and projects");
    expect(flat.lit[0].color.red > flat.lit[3].color.red,
           "shared source corner receives different flat facet colors");
    expect(flat.lit[3].color.red == 60, "back-facing triangle is ambient-only");
    for(std::size_t i = 0; i < indices.size(); ++i) {
        expect(same(flat.base[i], source[indices[i]]), "lighting leaves unlit base vertex unchanged");
        expect(flat.lit[i].u == flat.base[i].u && flat.lit[i].v == flat.base[i].v &&
               same(flat.lit[i].position, flat.base[i].position),
               "lighting never changes UVs or source positions");
    }
    const auto saved_color = flat.lit[0].color.argb();
    expect(!flat.update({0, 0.5f, 0}), "light must be above whole model, not just current facet");
    expect(flat.lit[0].color.argb() == saved_color, "invalid height is rejected before frame update");
    expect(!flat.update({0, std::numeric_limits<float>::quiet_NaN(), 0}),
           "nonfinite model light is rejected");
    auto bad_indices = indices;
    bad_indices[5] = 4;
    expect(!flat.initialize(source, bad_indices), "out-of-range source index is rejected");
    expect(!flat.initialized, "failed initialization disables updates");
    auto invalid_source = source;
    invalid_source[0].u = std::numeric_limits<float>::infinity();
    expect(!flat.initialize(invalid_source, indices), "nonfinite source UV is rejected");
    invalid_source = source;
    invalid_source[0].position.y = -0.1f;
    expect(!flat.initialize(invalid_source, indices), "below-ground source geometry is rejected");
    bad_indices = {0, 0, 0, 0, 2, 3};
    expect(!flat.initialize(source, bad_indices), "degenerate source triangle is rejected");
}

void test_authored_asset() {
    constexpr auto index_count = maishuji::car_2cv_mesh::indices.size();
    using Car = lighting::FlatCar<index_count>;
    static_assert(Car::triangle_count == 498);
    static_assert(Car::vertex_count == 1494);
    static_assert(std::tuple_size_v<decltype(Car::base)> == 1494);
    static_assert(std::tuple_size_v<decltype(Car::lit)> == 1494);
    static_assert(std::tuple_size_v<decltype(Car::shadow)> == 1494);
    static_assert(std::tuple_size_v<decltype(Car::indices)> == 1494);
    static_assert(std::tuple_size_v<decltype(Car::facets)> == 498);
    static Car car;
    expect(car.initialize(maishuji::car_2cv_mesh::vertices,
                           maishuji::car_2cv_mesh::indices), "generated 2CV asset expands without decoder");
    if(!car.initialized) return;
    Vec3 minimum = car.base[0].position;
    Vec3 maximum = minimum;
    for(std::size_t i = 0; i < index_count; ++i) {
        const auto &source = maishuji::car_2cv_mesh::vertices[maishuji::car_2cv_mesh::indices[i]];
        expect(same(car.base[i], source) && same(car.lit[i], source),
               "initial expansion exactly preserves generated vertex, UV and base colors");
        expect(car.indices[i] == i, "expanded indices are sequential, not source-shared");
        const auto p = source.position;
        minimum = {std::min(minimum.x, p.x), std::min(minimum.y, p.y), std::min(minimum.z, p.z)};
        maximum = {std::max(maximum.x, p.x), std::max(maximum.y, p.y), std::max(maximum.z, p.z)};
        expect(source.u >= 0 && source.u <= 1 && source.v >= 0 && source.v <= 1,
               "generated normalized UVs need no additional conversion");
    }
    expect(minimum.y == 0, "authored tires contact y=0 without recentering");
    expect(near(maximum.x - minimum.x, 1.54f, 0.01f), "Q8.8 car width retains authored scale");
    expect(near(maximum.y - minimum.y, 1.612f, 0.01f), "Q8.8 car height retains authored scale");
    expect(near(maximum.z - minimum.z, 3.8f, 0.01f), "Q8.8 car length retains authored scale");
    for(std::size_t i = 0; i < Car::triangle_count; ++i) {
        const auto &a = car.base[i * 3].position;
        const auto &b = car.base[i * 3 + 1].position;
        const auto &c = car.base[i * 3 + 2].position;
        const auto expected_normal = maishuji::normalize(maishuji::cross(b - a, c - a));
        expect(same(car.facets[i].normal, expected_normal), "asset flat normal follows authored winding");
        expect(same(car.facets[i].centroid, (a + b + c) * (1.0f / 3.0f)),
               "asset centroid matches expanded positions");
    }
    // Sample the complete deterministic animation, including exact contact
    // points. This checks the receiver bounds, not a fabricated rendered image.
    bool inside_floor = true;
    bool unchanged_base_and_uv = true;
    bool uniform_shadow = true;
    bool contact_fixed = true;
    bool flat_colors = true;
    for(int frame = 0; frame < 720; ++frame) {
        const float angle = 2.0f * lighting::pi * static_cast<float>(frame) / 720.0f;
        expect(car.update(lighting::orbit_light(angle)), "full-orbit asset frame updates successfully");
        for(std::size_t i = 0; i < index_count; ++i) {
            const auto &source = maishuji::car_2cv_mesh::vertices[maishuji::car_2cv_mesh::indices[i]];
            const auto &shadow = car.shadow[i];
            inside_floor = inside_floor && lighting::finite(shadow.position) &&
                std::fabs(shadow.position.x) < lighting::ground_half_extent &&
                std::fabs(shadow.position.z) < lighting::ground_half_extent;
            uniform_shadow = uniform_shadow &&
                shadow.color.argb() == lighting::shadow_color.argb() &&
                shadow.position.y == lighting::shadow_height;
            unchanged_base_and_uv = unchanged_base_and_uv && same(car.base[i], source) &&
                same(car.lit[i].position, source.position) &&
                car.lit[i].u == source.u && car.lit[i].v == source.v &&
                car.lit[i].offset_color.argb() == source.offset_color.argb();
            if(source.position.y == 0)
                contact_fixed = contact_fixed && shadow.position.x == source.position.x &&
                                shadow.position.z == source.position.z;
            if(i % 3 != 0)
                flat_colors = flat_colors &&
                    car.lit[i].color.argb() == car.lit[i - i % 3].color.argb();
        }
    }
    expect(inside_floor, "all projected asset vertices remain on the 14x14 receiver throughout orbit");
    expect(uniform_shadow, "all shadow triangles use one opaque color and the same render bias");
    expect(unchanged_base_and_uv, "720 updates never accumulate lighting or modify generated geometry/UVs");
    expect(contact_fixed, "asset ground-contact vertices do not drift with rotating light");
    expect(flat_colors, "all three vertices of each authored facet have identical lighting color");

    // An angle-independent conservative bound complements discrete orbit samples.
    const float projection_scale = car.maximum_y / (lighting::light_height - car.maximum_y);
    const float bound_x = std::max(std::fabs(minimum.x), std::fabs(maximum.x));
    const float bound_z = std::max(std::fabs(minimum.z), std::fabs(maximum.z));
    expect(bound_x + (bound_x + lighting::orbit_radius) * projection_scale < lighting::ground_half_extent &&
           bound_z + (bound_z + lighting::orbit_radius) * projection_scale < lighting::ground_half_extent,
           "floor contains the shadow at every angle, not only sampled frames");

    // Exercise the actual capture poses with the same local lighting helpers
    // and framework transforms used by public mesh submission. No renderer or
    // clipping implementation is duplicated here.
    expect(lighting::capture_camera.valid(), "shared capture camera is valid");
    static_assert(lighting::capture_transforms.size() == lighting::capture_lights.size());
    for(std::size_t instance = 0; instance < lighting::capture_transforms.size(); ++instance) {
        const auto model = maishuji::transform_matrix(lighting::capture_transforms[instance]);
        const auto mvp = maishuji::view_projection_matrix(lighting::capture_camera) * model;
        const auto light = lighting::capture_lights[instance];
        const auto world_light = maishuji::to_vec3(model * maishuji::to_vec4(light));
        expect(car.update(light), "capture local light updates expanded geometry");
        bool capture_inside_floor = true;
        bool projection_equivalent = true;
        bool framed = true;
        float min_x = 1, max_x = -1, min_y = 1, max_y = -1;
        for(std::size_t i = 0; i < index_count; ++i) {
            const auto world_point = maishuji::to_vec3(model * maishuji::to_vec4(car.base[i].position));
            auto world_shadow = maishuji::to_vec3(model * maishuji::to_vec4(car.shadow[i].position));
            capture_inside_floor = capture_inside_floor && near(world_shadow.y, lighting::shadow_height) &&
                std::fabs(world_shadow.x) < lighting::ground_half_extent &&
                std::fabs(world_shadow.z) < lighting::ground_half_extent;
            Vec3 expected{};
            world_shadow.y = 0;
            projection_equivalent = projection_equivalent &&
                lighting::project_to_ground(world_point, world_light, expected) &&
                same(expected, world_shadow, 2.0e-5f);
            for(const auto local : {car.base[i].position, car.shadow[i].position}) {
                const auto projected = maishuji::project_point(mvp, local);
                const auto p = projected.normalized_device;
                framed = framed && projected.valid && lighting::finite(p) &&
                    std::fabs(p.x) < 1 && p.y > -5.0f / 6.0f && p.y < 11.0f / 15.0f &&
                    p.z >= -1 && p.z <= 1;
            }
            const auto p = maishuji::project_point(mvp, car.base[i].position).normalized_device;
            min_x = std::min(min_x, p.x);
            max_x = std::max(max_x, p.x);
            min_y = std::min(min_y, p.y);
            max_y = std::max(max_y, p.y);
        }
        expect(capture_inside_floor, "translated, yawed capture shadows fit the one world ground");
        expect(projection_equivalent, "transforming local shadow matches projecting from transformed light");
        expect(framed, "both capture cars and shadows fit between the title/footer bars");
        expect(std::max((max_x - min_x) * 320, (max_y - min_y) * 240) >= 100,
               "each capture car spans at least 100 pixels in one dimension");
        // A box enclosing the octahedron also checks marker/header separation.
        bool marker_framed = true;
        for(float x : {-0.2f, 0.2f}) {
            for(float y : {-0.2f, 0.2f}) {
                for(float z : {-0.2f, 0.2f}) {
                    const auto projected = maishuji::project_point(mvp, light + Vec3{x, y, z});
                    const auto p = projected.normalized_device;
                    marker_framed = marker_framed && projected.valid && lighting::finite(p) &&
                        std::fabs(p.x) < 1 && p.y > -5.0f / 6.0f && p.y < 11.0f / 15.0f &&
                        p.z >= -1 && p.z <= 1;
                }
            }
        }
        expect(marker_framed, "transformed capture light marker stays clear of labels");
    }
}

void test_normal_camera_marker_orbit() {
    const auto vp = maishuji::view_projection_matrix(lighting::normal_camera);
    bool framed = true;
    for(int phase = 0; phase < 720; ++phase) {
        const auto light = lighting::orbit_light(2.0f * lighting::pi * phase / 720.0f);
        // Conservatively enclose the complete octahedron, not only its center.
        for(float x : {-0.2f, 0.2f})
            for(float y : {-0.2f, 0.2f})
                for(float z : {-0.2f, 0.2f}) {
                    const auto projected = maishuji::project_point(vp, light + Vec3{x, y, z});
                    const auto p = projected.normalized_device;
                    framed = framed && projected.valid && lighting::finite(p) &&
                        std::fabs(p.x) < 1 && p.y < 5.0f / 6.0f &&
                        p.y > -5.0f / 6.0f && p.z >= -1 && p.z <= 1;
                }
    }
    expect(framed, "full normal-mode light orbit stays inside the view and clear of label bars");
}

void test_shadow_configuration() {
    expect(lighting::shadow_list == maishuji::List::Opaque,
           "actual shared shadow list constant is opaque, not translucent");
    expect(lighting::shadow_configuration.culling == maishuji::Culling::None,
           "projected triangles survive either winding");
    expect(lighting::shadow_configuration.depth_compare == maishuji::DepthCompare::GreaterOrEqual,
           "coplanar overlapping shadow triangles may replace the same color");
    expect(lighting::shadow_configuration.depth_write,
           "shadow writes depth instead of accumulating translucent coverage");
    expect(!lighting::shadow_configuration.enable_offset_color && lighting::shadow_color.alpha == 255,
           "shadow has no additive offset or partial opacity");
    expect(lighting::shadow_height > 0 && lighting::shadow_height < 0.005f,
           "small receiver bias separates shadow from the flat ground");
}


} // namespace

int main() {
    test_facets_and_lambert();
    test_projection();
    test_shared_vertex_expansion();
    test_authored_asset();
    test_shadow_configuration();
    test_normal_camera_marker_orbit();

    if(failures != 0) {
        std::fprintf(stderr, "dynamic lighting: %d failure(s)\n", failures);
        return 1;
    }
    std::puts("dynamic lighting: all host checks passed");
    return 0;
}
