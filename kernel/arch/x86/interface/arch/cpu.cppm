export module arch.cpu;
import gloxor.types;

export namespace arch {

inline void spinloop_hint()
{
    asm("pause");
}

inline void halt()
{
    asm("hlt");
}

void initialize_cpu();

[[noreturn]] inline void halt_forever()
{
    while (1) {
        asm("cli;hlt");
    }
}

enum class FeatureBit
{
    avx,
    xsave,
};
constexpr FeatureBit operator|(FeatureBit l, FeatureBit r)
{
    return FeatureBit(static_cast<u64>(l) | static_cast<u64>(r));
}

bool is_feature_supported(FeatureBit);
} // namespace arch
