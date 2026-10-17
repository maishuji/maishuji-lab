#include "input_backend.hpp"

#include <array>
#include <cstddef>

namespace maishuji {
namespace {
std::array<ControllerSnapshot, 4> fixtures{};
}

ControllerSnapshot sample_controller(ControllerPort port) noexcept {
    const auto index = static_cast<std::size_t>(port);
    if(index >= fixtures.size()) {
        ControllerSnapshot invalid{};
        invalid.port = port;
        invalid.availability = InputAvailability::InvalidPort;
        return invalid;
    }
    ControllerSnapshot result = fixtures[index];
    result.port = port;
    if(!result.valid()) {
        result.buttons = 0;
        result.stick_x = result.stick_y = 0;
        result.left_trigger = result.right_trigger = 0;
        result.capabilities = {};
    }
    return result;
}

namespace host {
void set_controller_fixture(ControllerPort port, ControllerSnapshot snapshot) noexcept {
    const auto index = static_cast<std::size_t>(port);
    if(index < fixtures.size()) fixtures[index] = snapshot;
}

void clear_controller_fixtures() noexcept {
    fixtures = {};
}
} // namespace host
} // namespace maishuji
