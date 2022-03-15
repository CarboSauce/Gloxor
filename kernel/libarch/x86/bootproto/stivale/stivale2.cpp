#include "stivale2.h"
#include "glox/utilalgs.hpp"
#include "memory/pmm.hpp"
#include "protos/egg.h"
#include "system/terminal.hpp"
#include <new>
#include <stddef.h>
#include <arch/types.hpp>

using namespace glox;
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

__attribute__((section(".stivale2hdr"), used)) volatile struct stivale2_header header2 = {
	.entry_point = (uint64_t)stivale2_main,
	.stack = (uintptr_t)stacks[0] + sizeof(stack),
	.flags = (1 << 1) | (1<<2) | (1<<3) ,
	.tags = (uint64_t)&framebuffer_request};

struct eggHandle eggFrame;
extern char _kernelFileBegin[];
extern char _kernelFileEnd[];
u64 kernelMappingOffset;
u64 kernelPhysOffset;
u64 kernelVirtOffset;
u64 physicalFbStart;
u64 physicalFbEnd;
extern "C" void stivale2_main(struct stivale2_struct* info)
{
	// Print the tags.
	struct stivale2_tag* tag = (struct stivale2_tag*)info->tags;

	struct stivale2_struct_tag_framebuffer* fbuff;
	// We need to initialize memory after everything else, otherwise we might corrupt bootloader reclaimable
	stivale2_struct_tag_memmap* m{};
	bool isFullVirtual = false;
	while (tag != NULL)
	{
		switch (tag->identifier)
		{
			case STIVALE2_STRUCT_TAG_FRAMEBUFFER_ID:
			{
				fbuff = (struct stivale2_struct_tag_framebuffer*)tag;
				auto fb_start = (void*)fbuff->framebuffer_addr;
				auto fb_end = (void*)(fbuff->framebuffer_addr + fbuff->framebuffer_pitch * fbuff->framebuffer_height);
				auto height = fbuff->framebuffer_height;
				auto pitch = fbuff->framebuffer_pitch / 4;
				auto width = fbuff->framebuffer_width;
				glox::term::initTerm((colorT*)fb_start, (colorT*)fb_end, pitch, width, height);
				break;
			}
			case STIVALE2_STRUCT_TAG_MEMMAP_ID:
			{
				m = (stivale2_struct_tag_memmap*)tag;
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
			default:
				break;
		}

		tag = (struct stivale2_tag*)tag->next;
	}

	if (!isFullVirtual)
	{
		kernelPhysOffset = (u64)_kernelFileBegin-arch::kernelHalfBase;
		kernelVirtOffset = (u64)_kernelFileBegin;
		kernelMappingOffset = -arch::kernelHalfBase;
	}
	eggFrame.kInfo.start = _kernelFileBegin;
	eggFrame.kInfo.end = _kernelFileEnd;

	initializePmm(m);

	gloxorMain();
}

inline void initializePmm(stivale2_struct_tag_memmap* m)
{
	const auto* mMap = m->memmap;
	const auto entryCount = m->entries;
	size_t curIndex = 0;
	glox::pmmHeader* recentCtx = pmmCtx;
	// We gotta place first entry by hand because it doesnt follow the pattern
	for (; curIndex != entryCount; ++curIndex)
	{
		if (auto mTemp = mMap[curIndex];
			mTemp.type == STIVALE2_MMAP_USABLE || mTemp.type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE)
		{
			pmmCtx = glox::initPmm((void*)mTemp.base,mTemp.length, glox::pmmCtx);
			break;
		}
		else if (mMap[curIndex].type == STIVALE2_MMAP_FRAMEBUFFER)
		{
			physicalFbStart = mMap[curIndex].base;
			physicalFbEnd	 = mMap[curIndex].base+mMap[curIndex].length;
		}
	}
	for (; curIndex != entryCount; ++curIndex)
	{
		if (auto mTemp = mMap[curIndex];
			mTemp.type == STIVALE2_MMAP_USABLE || mTemp.type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE)
		{
			recentCtx->next = (glox::pmmHeader*)mTemp.base;
			recentCtx = recentCtx->next;
			recentCtx = glox::initPmm((void*)mTemp.base,mTemp.length, recentCtx);
		}
		else if (mMap[curIndex].type == STIVALE2_MMAP_FRAMEBUFFER)
		{
			physicalFbStart = mMap[curIndex].base;
			physicalFbEnd	 = mMap[curIndex].base+mMap[curIndex].length;
		}
	}
	recentCtx->next = nullptr;
}