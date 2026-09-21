#include <array>
#include <concepts>
#include <cstdio>
#include <span>
#include <type_traits>

template <typename T>
concept ScreenCoordinate = std::floating_point<T>;

template <ScreenCoordinate T>
constexpr T midpoint(T first, T second) {
    return (first + second) / static_cast<T>(2);
}

static_assert(__cplusplus >= 202002L);
static_assert(midpoint(2.0f, 6.0f) == 4.0f);

int main() {
    constexpr std::array samples{1, 2, 3};
    const std::span<const int> view{samples};
    if(view.size() != 3)
        return 1;

    std::puts("C++20 host compiler ready");
    return view[0] + view[1] + view[2] == 6 ? 0 : 1;
}
