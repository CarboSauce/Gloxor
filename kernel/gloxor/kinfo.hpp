#pragma once
#include <gloxor/types.hpp>
#include <glox/util.hpp>

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

		glox::span<memoryMap> mmapEntries;
		fbInfo fbInfoEntry;
		glox::span<u8> kernelCode;

	};
	inline glox::bootInfo machineInfo;
} // namespace glox