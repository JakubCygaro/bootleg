#ifndef DEFER_HPP
#define DEFER_HPP
#include <functional>

#define CONCAT(a, b) CONCAT_INNER(a, b)
#define CONCAT_INNER(a, b) a##b

#ifdef __COUNTER__
#define UNIQUE_NAME(base) CONCAT(base, __COUNTER__)
#else
#define UNIQUE_NAME(base) CONCAT(base, __LINE__)
#endif

#define DEFER(BLOCK) auto UNIQUE_NAME(defer) = defer([&](void) { BLOCK; })

class defer final {
public:
    using defer_fn_t = std::function<void(void)>;

private:
    defer_fn_t m_fn;

public:
    explicit inline defer(defer_fn_t&& fn) noexcept
        : m_fn(fn) { };

    inline ~defer()
    {
        m_fn();
    }
};
#endif
