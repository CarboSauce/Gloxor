#include "stivale2.h"
#include "arch/addrspace.hpp"
#include "arch/archinfo.hpp"
#include "gloxor/kinfo.hpp"
#include "memory/pmm.hpp"
#include "protos/egg.h"
#include "system/terminal.hpp"

using namespace glox;
using namespace arch;
// Abi requires for stack to be 16byte aligned,
// not like it matters on x86 but better make it aligned
typedef uint8_t stack[4096];
alignas(16) static stack stacks[10] = {};
extern "C" void stivale2_main(struct stivale2_struct* info);
extern "C" void gloxorMain();

struct stivale2_header_tag_framebuffer framebuffer_request = {
	 .tag = {
		  .identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID,
		  .next = 0},
	 .framebuffer_width = 0,
	 .framebuffer_height = 0,
	 .framebuffer_bpp = 0,
};

inline void initializePmm(stivale2_struct_tag_memmap*);
inline void setupKernelmemmap(stivale2_struct_tag_memmap*);

__attribute__((section(".stivale2hdr"), used)) struct stivale2_header header2 = {
	 .entry_point = (uint64_t)stivale2_main,
	 .stack = (uintptr_t)stacks[0] + sizeof(stack),
	 .flags = (1 << 1) | (1 << 2) | (1 << 3),
	 .tags = (uint64_t)&framebuffer_request};

// u64 physicalFbStart;
// u64 physicalFbEnd;
extern "C" void stivale2_main(stivale2_struct* info)
{
	// Print the tags.
	auto* tag = (stivale2_tag*)info->tags;

	stivale2_struct_tag_framebuffer* fbuff;
	// We need to initialize memory after everything else, otherwise we might corrupt bootloader reclaimable
	stivale2_struct_tag_memmap* m{};
	bool isFullVirtual = false;
	while (tag != nullptr)
	{
		switch (tag->identifier)
		{
			case STIVALE2_STRUCT_TAG_FRAMEBUFFER_ID:
			{
				fbuff = (stivale2_struct_tag_framebuffer*)tag;
				auto fb_start = fbuff->framebuffer_addr;
				auto fb_end = fbuff->framebuffer_addr + fbuff->framebuffer_pitch * fbuff->framebuffer_height;
				auto height = fbuff->framebuffer_height;
				auto pitch = fbuff->framebuffer_pitch / 4;
				auto width = fbuff->framebuffer_width;
				machineInfo.fbInfoEntry.begin = fb_start;
				machineInfo.fbInfoEntry.end = fb_end;
				machineInfo.fbInfoEntry.height = height;
				machineInfo.fbInfoEntry.pitch = pitch;
				machineInfo.fbInfoEntry.width = width;
				glox::term::initTerm((colorT*)fb_start, (colorT*)fb_end, pitch, width, height);
				break;
			}
			case STIVALE2_STRUCT_TAG_MEMMAP_ID:
			{
				m = (stivale2_struct_tag_memmap*)tag;
				initializePmm(m);
				break;
			}
			case STIVALE2_STRUCT_TAG_KERNEL_BASE_ADDRESS_ID:
			{
				auto baseaddr = (stivale2_struct_tag_kernel_base_address*)tag;
				kernelPhysOffset = baseaddr->physical_base_address;
				kernelVirtOffset = baseaddr->virtual_base_address;
				kernelMappingOffset = kernelPhysOffset - kernelVirtOffset;
				isFullVirtual = true;
				break;
			}
			case STIVALE2_STRUCT_TAG_RSDP_ID:
			{
				arch::archInfo.acpiRsdp =
					 reinterpret_cast<stivale2_struct_tag_rsdp*>(tag)->rsdp;
			}
			default:
				break;
		}

		tag = (stivale2_tag*)tag->next;
	}

	if (!isFullVirtual)
	{
		kernelPhysOffset = (u64)kernelFileBegin - arch::kernelMemBase;
		kernelVirtOffset = (u64)kernelFileBegin;
		kernelMappingOffset = -arch::kernelMemBase;
	}
	machineInfo.kernelCode = span(kernelFileBegin, kernelFileEnd);
	setupKernelmemmap(m);

	gloxorMain();
}

inline void initializePmm(stivale2_struct_tag_memmap* m)
{
	const auto* mMap = m->memmap;
	const auto entryCount = m->entries;
	for (size_t curIndex = 0; curIndex != entryCount; ++curIndex)
	{
		// for now reclaiming bootloader memory is too big of a risk as it can corrupt page tables
		// once memory manager can handle it, do it
		if (auto mTemp = mMap[curIndex];
			 mTemp.type == STIVALE2_MMAP_USABLE) //|| mTemp.type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE)
		{
			glox::pmmAddChunk(reinterpret_cast<void*>(mTemp.base + arch::physicalMemBase), mTemp.length);
		}
		/*else if (mMap[curIndex].type == STIVALE2_MMAP_FRAMEBUFFER)
		{
			physicalFbStart = mMap[curIndex].base;
			physicalFbEnd	 = mMap[curIndex].base+mMap[curIndex].length;
		}*/
	}
}

inline bootInfo::memTypes convertMemTypes(u32 type)
{
	using mtype = bootInfo::memTypes;
	switch (type)
	{
		case STIVALE2_MMAP_USABLE:
			return mtype::usable;
		case STIVALE2_MMAP_RESERVED:
			return mtype::reserved;
		case STIVALE2_MMAP_ACPI_NVS:
			return mtype::acpiNvs;
		case STIVALE2_MMAP_ACPI_RECLAIMABLE:
			return mtype::acpiReclaim;
		case STIVALE2_MMAP_BAD_MEMORY:
			return mtype::badMem;
		case STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE:
			return mtype::reclaimable;
		case STIVALE2_MMAP_FRAMEBUFFER:
			return mtype::framebuffer;
		case STIVALE2_MMAP_KERNEL_AND_MODULES:
			return mtype::kernel;
		default:
			gloxUnreachable();
	}
}

inline void setupKernelmemmap(stivale2_struct_tag_memmap* m)
{
	const auto* mMap = m->memmap;
	const auto entryCount = m->entries;
	auto* memmap = new bootInfo::memoryMap[entryCount];
	for (size_t i = 0; i != entryCount; ++i)
	{
		auto mTemp = mMap[i];
		memmap[i] = {.base = mTemp.base,
						 .length = mTemp.length,
						 .type = convertMemTypes(mTemp.type)};
	}
	machineInfo.mmapEntries = span<bootInfo::memoryMap>(memmap, memmap + entryCount);
}
