#pragma once
#include <gloxor/types.hpp>

namespace arch {
struct Kinfo {
	paddrT acpiRsdp;
};
inline arch::Kinfo archInfo; // meant to be used by arch specific code
} // namespace arch