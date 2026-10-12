export module arch.archinfo;
import gloxor.types;

export namespace arch {
struct Kinfo
{
    vaddr acpiRsdp;
};
inline arch::Kinfo archInfo; // meant to be used by arch specific code
} // namespace arch
