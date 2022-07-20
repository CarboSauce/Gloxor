#pragma once
#include <gloxor/types.hpp>

namespace arch
{
	struct kinfo
	{
		paddrT acpiRsdp;
	};
	inline arch::kinfo archInfo; // meant to be used by arch specific code
}