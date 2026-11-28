export module arch.percpu;
import std.compat;
import glox;
import arch_asm.msr;
import gloxor.system.logging;

extern "C"
{
extern uint8_t __percpu_load[];
extern uint8_t __percpu_start[];
extern uint8_t __percpu_end[];
}

export namespace arch {

[[nodiscard]] uintptr_t gs_base() noexcept
{
    uintptr_t base;
    asm volatile("mov %%gs:0, %0" : "=r"(base));
    return base;
}

[[nodiscard]] size_t percpu_area_size()
{
    return static_cast<size_t>(__percpu_end - __percpu_start);
}

void initialize_percpu(void* buf, size_t s)
{
    glox::memcpy(buf, __percpu_start, s);
    uintptr_t const base = reinterpret_cast<uintptr_t>(buf);
    // monkey patch self pointer
    *reinterpret_cast<uintptr_t volatile*>(buf) = base;

    msr::wrmsr(msr::IA32_GS_BASE, base);
}

template <typename T>
struct Percpu
{
    T val;

    class Proxy
    {
        uintptr_t offset_;

    public:
        explicit constexpr Proxy(uintptr_t offset) noexcept
            : offset_(offset)
        {
        }

        [[nodiscard]] operator T() const noexcept
        {
            if constexpr (
                sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4
                || sizeof(T) == 8
            ) {
                T out;
                asm volatile("mov %%gs:(%1), %0"
                    : "=r"(out)
                    : "r"(offset_)
                    : "memory");
                return out;
            } else {
                return *reinterpret_cast<T const*>(gs_base() + offset_);
            }
        }

        Proxy& operator=(T const& rhs) noexcept
        {
            if constexpr (
                sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4
                || sizeof(T) == 8
            ) {
                asm volatile("mov %0, %%gs:(%1)"
                    :
                    : "r"(rhs), "r"(offset_)
                    : "memory");
            } else {
                *reinterpret_cast<T*>(gs_base() + offset_) = rhs;
            }
            return *this;
        }
    };

    [[nodiscard]] uintptr_t offset() const noexcept
    {
        return reinterpret_cast<uintptr_t>(&val)
             - reinterpret_cast<uintptr_t>(__percpu_start);
    }

    [[nodiscard]] T operator*() const noexcept
    {
        return static_cast<T>(Proxy { offset() });
    }

    [[nodiscard]] Proxy operator*() noexcept
    {
        return Proxy { offset() };
    }

    [[nodiscard]] T* operator->() noexcept
    {
        return reinterpret_cast<T*>(gs_base() + offset());
    }

    [[nodiscard]] T const* operator->() const noexcept
    {
        return reinterpret_cast<T const*>(gs_base() + offset());
    }

    [[nodiscard]] T* ptr() noexcept
    {
        return reinterpret_cast<T*>(gs_base() + offset());
    }

    [[nodiscard]] T const* ptr() const noexcept
    {
        return reinterpret_cast<T const*>(gs_base() + offset());
    }
};
} // namespace arch
