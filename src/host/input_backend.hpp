#pragma once

#include "maishuji/input.hpp"

namespace maishuji::host {

/// Supplies one host-only test fixture for a physical slot, without Maple.
void set_controller_fixture(ControllerPort port, ControllerSnapshot snapshot) noexcept;

/// Resets all host-only fixtures to disconnected.
void clear_controller_fixtures() noexcept;

} // namespace maishuji::host
