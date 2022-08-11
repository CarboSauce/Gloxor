#pragma once
#include <glox/util.hpp>
#include <gloxor/types.hpp>

namespace glox
{
struct BootInfo
{
	enum class MemTypes
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
	struct MemoryMap
	{
		paddrT base;
		sizeT length;
		MemTypes type;
	};
	struct FbInfo
	{
		paddrT begin;
		paddrT end;
		sizeT pitch;
		sizeT height;
		sizeT width;
	};

	glox::span<MemoryMap> mmapEntries;
	FbInfo fbInfoEntry;
	glox::span<u8> kernelCode;
};
inline glox::BootInfo machineInfo;
} // namespace glox