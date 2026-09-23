#include "recording_backend.hpp"

#include "detail/backend.hpp"

namespace maishuji::test {
namespace {

Recording state{};
FailurePoint next_failure = FailurePoint::None;

bool consume_failure(FailurePoint point) noexcept {
    if(next_failure != point)
        return true;

    next_failure = FailurePoint::None;
    return false;
}

bool initialize(const Configuration &configuration) noexcept {
    ++state.initialize_calls;
    state.configuration = configuration;
    return consume_failure(FailurePoint::Initialize);
}

bool wait_ready() noexcept {
    ++state.wait_ready_calls;
    return consume_failure(FailurePoint::WaitReady);
}

bool scene_begin() noexcept {
    ++state.scene_begin_calls;
    if(!consume_failure(FailurePoint::SceneBegin))
        return false;

    state.scene_open = true;
    return true;
}

bool scene_finish() noexcept {
    ++state.scene_finish_calls;
    state.scene_open = false;
    return consume_failure(FailurePoint::SceneFinish);
}

bool list_begin(List list) noexcept {
    ++state.list_begin_calls;
    state.last_list = list;
    if(!consume_failure(FailurePoint::ListBegin))
        return false;

    state.list_open = true;
    return true;
}

bool list_finish() noexcept {
    ++state.list_finish_calls;
    state.list_open = false;
    return consume_failure(FailurePoint::ListFinish);
}

bool wait_render_done() noexcept {
    ++state.render_wait_calls;
    return consume_failure(FailurePoint::RenderWait);
}

bool shutdown() noexcept {
    ++state.shutdown_calls;
    return consume_failure(FailurePoint::Shutdown);
}

} // namespace

void reset_recording() noexcept {
    state = {};
    next_failure = FailurePoint::None;
}

void fail_next(FailurePoint point) noexcept {
    next_failure = point;
}

const Recording &recording() noexcept {
    return state;
}

} // namespace maishuji::test

namespace maishuji::detail {

const Backend &default_backend() noexcept {
    static const Backend backend{
        maishuji::test::initialize,
        maishuji::test::wait_ready,
        maishuji::test::scene_begin,
        maishuji::test::scene_finish,
        maishuji::test::list_begin,
        maishuji::test::list_finish,
        maishuji::test::wait_render_done,
        maishuji::test::shutdown,
    };
    return backend;
}

} // namespace maishuji::detail
