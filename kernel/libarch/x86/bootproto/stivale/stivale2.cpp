#include "stivale2.h"
#include "glox/utilalgs.hpp"
#include "memory/pmm.hpp"
#include "protos/egg.h"
#include <new>
#include <stddef.h>


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

__attribute__((section(".stivale2hdr"), used)) volatile struct stivale2_header header2 = {
	.entry_point = (uint64_t)stivale2_main,
	.stack = (uintptr_t)stacks[0] + sizeof(stack),
	.flags = 1 << 1,
	.tags = (uint64_t)&framebuffer_request};

struct eggHandle eggFrame;
extern char _kernelFileBegin[];
extern char _kernelFileEnd[];
extern "C" void stivale2_main(struct stivale2_struct* info)
{
	// Print the tags.
	struct stivale2_tag* tag = (struct stivale2_tag*)info->tags;

	struct stivale2_struct_tag_framebuffer* fbuff;

	while (tag != NULL)
	{
		switch (tag->identifier)
		{
			case STIVALE2_STRUCT_TAG_FRAMEBUFFER_ID:
				fbuff = (struct stivale2_struct_tag_framebuffer*)tag;
				eggFrame.fb.fb_start = (void*)fbuff->framebuffer_addr;
				eggFrame.fb.fb_end = (void*)(fbuff->framebuffer_addr + fbuff->framebuffer_pitch * fbuff->framebuffer_height);
				eggFrame.fb.height = fbuff->framebuffer_height;
				eggFrame.fb.pitch = fbuff->framebuffer_pitch / 4;
				eggFrame.fb.width = fbuff->framebuffer_width;
				break;
			case STIVALE2_STRUCT_TAG_MEMMAP_ID:
			{
				stivale2_struct_tag_memmap* m = (stivale2_struct_tag_memmap*)tag;
				const auto* mMap = m->memmap;
				const auto entryCount = m->entries;
				eggFrame.memMap.base = (eggMemoryMapEntry*)mMap;
				eggFrame.memMap.size = entryCount;
				size_t curIndex = 0;
				glox::pmmHeader* recentCtx = pmmCtx;
				auto initPmm = [&](stivale2_mmap_entry entry, glox::pmmHeader* ctx)
				{
					size_t s = entry.length ;
					ctx = new ((void*)entry.base) glox::pmmHeader;
					ctx->size = s;
					ctx->start = (void*)(entry.base + sizeof(glox::pmmHeader) + s/(8*4096));
					std::uninitialized_default_construct_n(ctx->bitmap, s);
					return ctx;
				};
				// We gotta place first entry by hand because it doesnt follow the pattern
				for (; curIndex != entryCount; ++curIndex)
				{
					if (auto mTemp = mMap[curIndex];
						mTemp.type == STIVALE2_MMAP_USABLE || mTemp.type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE)
						{
							pmmCtx = initPmm(mTemp, glox::pmmCtx);
							break;
						}
				}
				for (; curIndex != entryCount; ++curIndex)
				{
					if (auto mTemp = mMap[curIndex];
						mTemp.type == STIVALE2_MMAP_USABLE || mTemp.type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE)
					{
						recentCtx->next = (glox::pmmHeader*)mTemp.base;
						recentCtx = recentCtx->next;
						recentCtx = initPmm(mTemp, recentCtx);
					}
				}
				recentCtx->next = nullptr;
			}
			default:
				break;
		}

		tag = (struct stivale2_tag*)tag->next;
	}
	eggFrame.kInfo.start = _kernelFileBegin;
	eggFrame.kInfo.end = _kernelFileEnd;

	gloxorMain();
}
