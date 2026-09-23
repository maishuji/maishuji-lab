#pragma once

#include "maishuji/pvr.hpp"

namespace maishuji::detail {

struct Backend {
    bool (*initialize)(const Configuration &configuration) noexcept;
    bool (*wait_ready)() noexcept;
    bool (*scene_begin)() noexcept;
    bool (*scene_finish)() noexcept;
    bool (*list_begin)(List list) noexcept;
    bool (*list_finish)() noexcept;
    bool (*wait_render_done)() noexcept;
    bool (*shutdown)() noexcept;
};

const Backend &default_backend() noexcept;

} // namespace maishuji::detail
