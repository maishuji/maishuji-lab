#pragma once

#include "maishuji/pvr.hpp"

#include <cstddef>
#include <cstdint>

namespace maishuji::test {

enum class FailurePoint : std::uint8_t {
    None,
    Initialize,
    WaitReady,
    SceneBegin,
    SceneFinish,
    ListBegin,
    ListFinish,
    RenderWait,
    Shutdown,
};

struct Recording {
    std::size_t initialize_calls = 0;
    std::size_t wait_ready_calls = 0;
    std::size_t scene_begin_calls = 0;
    std::size_t scene_finish_calls = 0;
    std::size_t list_begin_calls = 0;
    std::size_t list_finish_calls = 0;
    std::size_t render_wait_calls = 0;
    std::size_t shutdown_calls = 0;

    Configuration configuration{};
    List last_list = List::Opaque;
    bool scene_open = false;
    bool list_open = false;
};

void reset_recording() noexcept;
void fail_next(FailurePoint point) noexcept;
const Recording &recording() noexcept;

} // namespace maishuji::test
