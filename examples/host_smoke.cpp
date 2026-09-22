#include <array>
#include <concepts>
#include <cstdio>
#include <utility>
#include <span>
#include <type_traits>

template <typename T>
concept ScreenCoordinate = std::floating_point<T>;

template <ScreenCoordinate T>
constexpr T midpoint(T first, T second) {
    return (first + second) / static_cast<T>(2);
}

class MoveOnlyScope {
public:
    explicit MoveOnlyScope(int *destructions) noexcept
        : destructions_(destructions) {}

    MoveOnlyScope(const MoveOnlyScope &) = delete;
    MoveOnlyScope &operator=(const MoveOnlyScope &) = delete;

    MoveOnlyScope(MoveOnlyScope &&other) noexcept
        : destructions_(other.destructions_) {
        other.destructions_ = nullptr;
    }

    MoveOnlyScope &operator=(MoveOnlyScope &&other) noexcept {
        if(this == &other)
            return *this;

        if(destructions_ != nullptr)
            ++*destructions_;
        destructions_ = other.destructions_;
        other.destructions_ = nullptr;
        return *this;
    }

    ~MoveOnlyScope() {
        if(destructions_ != nullptr)
            ++*destructions_;
    }

private:
    int *destructions_;
};

static_assert(__cplusplus >= 202002L);
static_assert(midpoint(2.0f, 6.0f) == 4.0f);
static_assert(std::movable<MoveOnlyScope>);
static_assert(!std::copy_constructible<MoveOnlyScope>);
static_assert(std::is_nothrow_move_constructible_v<MoveOnlyScope>);
static_assert(std::is_nothrow_move_assignable_v<MoveOnlyScope>);

int main() {
    constexpr std::array samples{1, 2, 3};
    const std::span<const int> view{samples};
    if(view.size() != 3)
        return 1;

    int destructions = 0;
    {
        MoveOnlyScope owner(&destructions);
        MoveOnlyScope moved(std::move(owner));
        (void)moved;
    }
    if(destructions != 1)
        return 1;

    std::puts("C++20 host compiler ready");
    return view[0] + view[1] + view[2] == 6 ? 0 : 1;
}
