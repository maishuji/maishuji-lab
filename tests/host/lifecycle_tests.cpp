#include "recording_backend.hpp"

#include "maishuji/pvr.hpp"

#include <array>
#include <cstdio>
#include <utility>

namespace {

int failures = 0;

void expect_status(maishuji::Status actual, maishuji::Status expected,
                   const char *label) {
    if(actual == expected)
        return;

    std::fprintf(stderr, "%s: expected %s, got %s\n",
                 label, maishuji::status_name(expected),
                 maishuji::status_name(actual));
    ++failures;
}

void expect_true(bool condition, const char *label) {
    if(condition)
        return;

    std::fprintf(stderr, "%s: condition failed\n", label);
    ++failures;
}

void expect_equal(std::size_t actual, std::size_t expected, const char *label) {
    if(actual == expected)
        return;

    std::fprintf(stderr, "%s: expected %zu, got %zu\n",
                 label, expected, actual);
    ++failures;
}

void test_basic_lifecycle() {
    using namespace maishuji;
    test::reset_recording();

    Pvr pvr;
    Frame first;
    Frame second;
    RenderList opaque;
    RenderList translucent;

    expect_status(pvr.begin_frame(first), Status::NotInitialized,
                  "begin before initialize");
    expect_status(pvr.initialize(), Status::Success, "initialize");
    expect_status(pvr.initialize(), Status::AlreadyInitialized,
                  "initialize twice");
    expect_status(pvr.begin_frame(first), Status::Success, "begin first frame");
    expect_status(pvr.begin_frame(second), Status::FrameAlreadyActive,
                  "reject nested frame");
    expect_status(first.begin_list(opaque, List::Opaque), Status::Success,
                  "begin opaque list");
    expect_status(first.begin_list(translucent, List::Translucent),
                  Status::RenderListAlreadyActive, "reject overlapping lists");
    expect_status(first.finish(), Status::RenderListActive,
                  "reject frame finish with active list");
    expect_status(opaque.finish(), Status::Success, "finish opaque list");
    expect_status(first.begin_list(opaque, List::Opaque),
                  Status::RenderListAlreadyFinished,
                  "reject reopening finished list");
    expect_status(opaque.finish(), Status::RenderListNotActive,
                  "reject list finish twice");
    expect_status(first.finish(), Status::Success, "finish frame");
    expect_status(first.finish(), Status::FrameNotActive,
                  "reject frame finish twice");
    expect_status(pvr.wait_render_done(), Status::Success,
                  "explicit render wait");
    expect_status(pvr.shutdown(), Status::Success, "shutdown");
    expect_equal(test::recording().wait_ready_calls, 1,
                 "wait-ready call count");
    expect_equal(test::recording().scene_begin_calls, 1,
                 "scene-begin call count");
    expect_equal(test::recording().list_begin_calls, 1,
                 "list-begin call count");
    expect_equal(test::recording().list_finish_calls, 1,
                 "list-finish call count");
    expect_equal(test::recording().scene_finish_calls, 1,
                 "scene-finish call count");
    expect_equal(test::recording().render_wait_calls, 2,
                 "render-wait call count");
    expect_equal(test::recording().shutdown_calls, 1,
                 "shutdown call count");
}

void test_configuration_and_disabled_list() {
    using namespace maishuji;
    test::reset_recording();

    Configuration invalid;
    invalid.vertex_buffer_bytes = 31;
    Pvr invalid_pvr;
    expect_status(invalid_pvr.initialize(invalid), Status::InvalidConfiguration,
                  "reject unaligned vertex buffer");

    Configuration configuration;
    configuration.enable_translucent = false;
    Pvr pvr;
    Frame frame;
    RenderList list;

    expect_status(pvr.initialize(configuration), Status::Success,
                  "initialize disabled-list configuration");
    expect_status(pvr.begin_frame(frame), Status::Success,
                  "begin disabled-list frame");
    expect_status(frame.begin_list(list, List::Translucent),
                  Status::RenderListDisabled, "reject disabled list");
    expect_status(frame.finish(), Status::Success, "finish disabled-list frame");
    expect_status(pvr.shutdown(), Status::Success, "shutdown disabled-list PVR");
}

void test_failed_acquisition_and_cleanup() {
    using namespace maishuji;

    test::reset_recording();
    Pvr pvr;
    expect_status(pvr.initialize(), Status::Success, "initialize failure test");

    Frame frame;
    RenderList list;

    test::fail_next(test::FailurePoint::WaitReady);
    expect_status(pvr.begin_frame(frame), Status::WaitReadyFailed,
                  "propagate ready-wait failure");
    expect_true(!frame.active(), "failed ready wait does not activate frame");
    expect_equal(test::recording().scene_begin_calls, 0,
                 "scene does not begin after ready-wait failure");

    test::fail_next(test::FailurePoint::SceneBegin);
    expect_status(pvr.begin_frame(frame), Status::SceneBeginFailed,
                  "propagate scene-begin failure");
    expect_true(!frame.active(), "failed scene begin does not activate frame");

    expect_status(pvr.begin_frame(frame), Status::Success,
                  "begin frame for list failure");
    test::fail_next(test::FailurePoint::ListBegin);
    expect_status(frame.begin_list(list, List::Opaque),
                  Status::RenderListBeginFailed, "propagate list-begin failure");
    expect_true(!list.active(), "failed list begin does not activate list");
    expect_status(frame.finish(), Status::Success, "finish after list-begin failure");

    expect_status(pvr.begin_frame(frame), Status::Success,
                  "begin frame for list-finish failure");
    expect_status(frame.begin_list(list, List::Opaque), Status::Success,
                  "begin list for list-finish failure");
    test::fail_next(test::FailurePoint::ListFinish);
    expect_status(list.finish(), Status::RenderListFinishFailed,
                  "propagate list-finish failure");
    expect_true(!list.active(), "failed list finish closes list state");
    expect_status(frame.finish(), Status::Success,
                  "finish frame after list-finish failure");

    expect_status(pvr.begin_frame(frame), Status::Success,
                  "begin frame for scene-finish failure");
    test::fail_next(test::FailurePoint::SceneFinish);
    expect_status(frame.finish(), Status::SceneFinishFailed,
                  "propagate scene-finish failure");
    expect_true(!frame.active(), "failed scene finish closes frame state");

    test::fail_next(test::FailurePoint::RenderWait);
    expect_status(pvr.shutdown(), Status::RenderWaitFailed,
                  "propagate shutdown render-wait failure");
    expect_true(!pvr.initialized(), "shutdown clears initialized state");

    test::reset_recording();
    test::fail_next(test::FailurePoint::Initialize);
    Pvr failed_pvr;
    expect_status(failed_pvr.initialize(), Status::BackendInitializationFailed,
                  "propagate initialization failure");
    expect_true(!failed_pvr.initialized(),
                "failed initialization does not activate PVR");
}

void test_scope_cleanup() {
    using namespace maishuji;
    test::reset_recording();

    Pvr pvr;
    expect_status(pvr.initialize(), Status::Success, "initialize scope test");

    Frame frame;
    RenderList list;
    expect_status(pvr.begin_frame(frame), Status::Success,
                  "begin scope-test frame");
    expect_status(frame.begin_list(list, List::Opaque), Status::Success,
                  "begin scope-test list");

    {
        Frame nested;
        expect_status(pvr.begin_frame(nested), Status::FrameAlreadyActive,
                      "reject second scope-test frame");
    }

    expect_true(frame.active(), "frame remains active before scope cleanup");
    expect_true(list.active(), "list remains active before scope cleanup");
    expect_status(pvr.shutdown(), Status::FrameActive,
                  "reject shutdown with active frame");

    expect_status(list.finish(), Status::Success, "finish scope-test list");
    expect_status(frame.finish(), Status::Success, "finish scope-test frame");
    expect_status(pvr.shutdown(), Status::Success, "shutdown scope-test PVR");

    test::reset_recording();
    Pvr destructor_pvr;
    expect_status(destructor_pvr.initialize(), Status::Success,
                  "initialize destructor test");
    Status frame_status = Status::NotInitialized;
    Status list_status = Status::NotInitialized;
    {
        Frame destructor_frame;
        RenderList destructor_list;
        expect_status(destructor_pvr.begin_frame(destructor_frame), Status::Success,
                      "begin destructor frame");
        expect_status(destructor_frame.begin_list(destructor_list, List::Opaque),
                      Status::Success, "begin destructor list");
        frame_status = destructor_frame.completion_status();
        list_status = destructor_list.completion_status();
    }
    expect_status(list_status, Status::Success,
                  "list destructor records successful close");
    expect_status(frame_status, Status::Success,
                  "frame destructor records successful close");
    expect_equal(test::recording().list_finish_calls, 1,
                 "destructor list finish call count");
    expect_equal(test::recording().scene_finish_calls, 1,
                 "destructor scene finish call count");
    expect_status(destructor_pvr.shutdown(), Status::Success,
                  "shutdown after destructor cleanup");
}

void test_texture_ownership() {
    using namespace maishuji;

    test::reset_recording();

    Texture texture;
    Pvr pvr;
    expect_status(texture.allocate(pvr, 8, 8), Status::NotInitialized,
                  "reject texture allocation before initialize");
    expect_status(pvr.initialize(), Status::Success,
                  "initialize texture test PVR");
    expect_status(texture.allocate(pvr, 3, 8),
                  Status::TextureInvalidDimensions,
                  "reject non-power-of-two texture dimensions");

    Texture allocation_failure;
    test::fail_next(test::FailurePoint::TextureAllocate);
    expect_status(allocation_failure.allocate(pvr, 8, 8),
                  Status::TextureAllocationFailed,
                  "propagate texture allocation failure");

    expect_status(texture.allocate(pvr, 8, 8), Status::Success,
                  "allocate texture");
    expect_status(texture.allocate(pvr, 8, 8),
                  Status::TextureAlreadyAllocated,
                  "reject duplicate texture allocation");

    std::array<std::uint16_t, 64> pixels{};
    expect_status(texture.upload(pixels), Status::Success,
                  "upload texture data");
    expect_equal(test::recording().last_texture_upload_bytes,
                 sizeof(pixels), "texture upload byte count");
    std::array<std::uint16_t, 63> short_pixels{};
    expect_status(texture.upload(short_pixels), Status::TextureInvalidData,
                  "reject short texture upload");
    test::fail_next(test::FailurePoint::TextureUpload);
    expect_status(texture.upload(pixels), Status::TextureUploadFailed,
                  "propagate texture upload failure");
    expect_status(texture.upload(pixels), Status::Success,
                  "retry texture upload");

    Texture moved(std::move(texture));
    expect_true(!texture.allocated(), "move construction clears source");
    expect_true(moved.allocated(), "move construction keeps allocation");
    Texture assigned;
    assigned = std::move(moved);
    expect_true(!moved.allocated(), "move assignment clears source");
    expect_true(assigned.allocated(), "move assignment keeps allocation");

    const TexturedQuad quad{
        {80.0f, 100.0f, 1.0f, 0.0f, 0.0f},
        {80.0f, 220.0f, 1.0f, 0.0f, 1.0f},
        {200.0f, 100.0f, 1.0f, 1.0f, 0.0f},
        {200.0f, 220.0f, 1.0f, 1.0f, 1.0f},
    };
    Texture unallocated;
    Frame frame;
    RenderList list;
    expect_status(list.submit(assigned, quad), Status::RenderListNotActive,
                  "reject textured submission outside list");
    expect_status(pvr.begin_frame(frame), Status::Success,
                  "begin texture frame");
    expect_status(frame.begin_list(list, List::Opaque), Status::Success,
                  "begin texture list");
    expect_status(list.submit(unallocated, quad), Status::TextureNotAllocated,
                  "reject unallocated texture submission");
    expect_status(list.submit(assigned, quad), Status::Success,
                  "submit textured quad");
    test::fail_next(test::FailurePoint::TexturedSubmit);
    expect_status(list.submit(assigned, quad),
                  Status::PrimitiveSubmissionFailed,
                  "propagate textured submission failure");
    expect_status(list.finish(), Status::Success, "finish texture list");
    expect_status(frame.finish(), Status::Success, "finish texture frame");

    Frame active_frame;
    expect_status(pvr.begin_frame(active_frame), Status::Success,
                  "begin active release frame");
    expect_status(assigned.release(), Status::FrameActive,
                  "reject texture release during active frame");
    expect_status(active_frame.finish(), Status::Success,
                  "finish active release frame");

    test::fail_next(test::FailurePoint::RenderWait);
    expect_status(assigned.release(), Status::RenderWaitFailed,
                  "propagate texture release wait failure");
    expect_true(assigned.allocated(),
                "retain texture allocation after release wait failure");
    expect_status(assigned.release(), Status::Success,
                  "release texture after render wait");
    expect_true(!assigned.allocated(), "release clears texture allocation");
    expect_status(pvr.shutdown(), Status::Success,
                  "shutdown texture test PVR");

    expect_equal(test::recording().texture_allocate_calls, 2,
                 "texture allocation call count");
    expect_equal(test::recording().texture_upload_calls, 3,
                 "texture upload call count");
    expect_equal(test::recording().texture_free_calls, 1,
                 "texture free call count");
    expect_equal(test::recording().textured_quad_submit_calls, 2,
                 "textured quad submission call count");
    expect_true(test::recording().last_primitive_list == List::Opaque,
                "textured list matches active list");
}

void test_texture_repeated_cycles() {
    using namespace maishuji;

    test::reset_recording();

    Pvr pvr;
    expect_status(pvr.initialize(), Status::Success,
                  "initialize repeated texture-cycle PVR");

    const std::array<std::uint16_t, 64> pixels{};
    const TexturedQuad quad{
        {80.0f, 100.0f, 1.0f, 0.0f, 0.0f},
        {80.0f, 220.0f, 1.0f, 0.0f, 1.0f},
        {200.0f, 100.0f, 1.0f, 1.0f, 0.0f},
        {200.0f, 220.0f, 1.0f, 1.0f, 1.0f},
    };

    constexpr int cycles = 8;
    for(int cycle = 0; cycle < cycles; ++cycle) {
        Texture texture;
        expect_status(texture.allocate(pvr, 8, 8), Status::Success,
                      "allocate repeated texture cycle");
        expect_status(texture.upload(pixels), Status::Success,
                      "upload repeated texture cycle");

        Frame frame;
        RenderList list;
        expect_status(pvr.begin_frame(frame), Status::Success,
                      "begin repeated texture frame");
        expect_status(frame.begin_list(list, List::Opaque), Status::Success,
                      "begin repeated texture list");
        expect_status(list.submit(texture, quad), Status::Success,
                      "submit repeated texture quad");
        expect_status(list.finish(), Status::Success,
                      "finish repeated texture list");
        expect_status(frame.finish(), Status::Success,
                      "finish repeated texture frame");
        expect_status(texture.release(), Status::Success,
                      "release repeated texture cycle");
    }

    expect_status(pvr.shutdown(), Status::Success,
                  "shutdown repeated texture-cycle PVR");
    expect_equal(test::recording().texture_allocate_calls, cycles,
                 "repeated texture allocation count");
    expect_equal(test::recording().texture_upload_calls, cycles,
                 "repeated texture upload count");
    expect_equal(test::recording().textured_quad_submit_calls, cycles,
                 "repeated textured submission count");
    expect_equal(test::recording().texture_free_calls, cycles,
                 "repeated texture free count");
    expect_equal(test::recording().render_wait_calls,
                 static_cast<std::size_t>(cycles + 1),
                 "repeated texture render-wait count");
}

void test_colored_primitives() {
    using namespace maishuji;

    test::reset_recording();

    const Triangle triangle{
        {320.0f, 88.0f, 1.0f, {255, 64, 64, 255}},
        {88.0f, 392.0f, 1.0f, {64, 255, 64, 255}},
        {552.0f, 392.0f, 1.0f, {64, 128, 255, 255}},
    };
    const Quad quad{
        {120.0f, 120.0f, 1.0f, {255, 255, 255, 255}},
        {120.0f, 300.0f, 1.0f, {255, 128, 64, 255}},
        {300.0f, 120.0f, 1.0f, {128, 192, 255, 255}},
        {300.0f, 300.0f, 1.0f, {255, 64, 192, 255}},
    };

    Pvr pvr;
    Frame frame;
    RenderList opaque;

    expect_status(pvr.initialize(), Status::Success,
                  "initialize primitive test");
    expect_status(opaque.submit(triangle), Status::RenderListNotActive,
                  "reject primitive outside list");
    expect_status(pvr.begin_frame(frame), Status::Success,
                  "begin primitive frame");
    expect_status(frame.begin_list(opaque, List::Opaque), Status::Success,
                  "begin primitive list");
    expect_status(opaque.submit(triangle), Status::Success,
                  "submit triangle");
    expect_status(opaque.submit(quad), Status::Success,
                  "submit quad");

    test::fail_next(test::FailurePoint::PrimitiveSubmit);
    expect_status(opaque.submit(triangle), Status::PrimitiveSubmissionFailed,
                  "propagate primitive failure");

    expect_status(opaque.finish(), Status::Success,
                  "finish primitive list");
    expect_status(frame.finish(), Status::Success,
                  "finish primitive frame");
    expect_status(pvr.shutdown(), Status::Success,
                  "shutdown primitive PVR");
    expect_equal(test::recording().triangle_submit_calls, 2,
                 "triangle submission call count");
    expect_equal(test::recording().quad_submit_calls, 1,
                 "quad submission call count");
    expect_true(test::recording().last_primitive_list == List::Opaque,
                "primitive list matches active list");
}

} // namespace

int main() {
    test_basic_lifecycle();
    test_configuration_and_disabled_list();
    test_failed_acquisition_and_cleanup();
    test_scope_cleanup();
    test_colored_primitives();
    test_texture_ownership();
    test_texture_repeated_cycles();

    if(failures != 0) {
        std::fprintf(stderr, "%d lifecycle test(s) failed\n", failures);
        return 1;
    }

    std::puts("maishuji lifecycle tests passed");
    return 0;
}
