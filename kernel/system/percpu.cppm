export module gloxor.system.percpu;
import arch.percpu;
import gloxor.types;

export namespace gx {
struct Cpu
{
    Cpu* self;
    u32 cpuId;
};

[[gnu::section(".percpu.head"), gnu::used]]
constinit arch::Percpu<Cpu> currentCpu {
    .val = { .self = nullptr, .cpuId = 0 },
};
} // namespace gx
