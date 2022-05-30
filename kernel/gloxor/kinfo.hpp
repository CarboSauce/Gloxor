#pragma once
#include "gloxor/graphics.hpp"
#include <gloxor/types.hpp>
#include <glox/util.hpp>
#include <arch/kinfo.hpp>

namespace arch { struct kinfo; }
namespace glox
{
	struct bootInfo
	{
		enum class memTypes
		{
			usable,
			reserved,
			acpiReclaim,
			acpiNvs,
			badMem,
			reclaimable,
			kernel,
			framebuffer
		};
		struct memoryMap
		{
			paddrT base;
			sizeT length;
			memTypes type;
		};
		struct fbInfo
		{
			paddrT begin;
			paddrT end;
			sizeT pitch;
			sizeT height;
			sizeT width;
		};

		memoryMap* mmapEntries;
		sizeT mmapSize;
		fbInfo fbInfoEntry;
		glox::span<u8> kernelCode;
		arch::kinfo archInfo; // meant to be used by arch specific code

	};
	inline glox::bootInfo machineInfo;
} // namespace glox