#include "stivale2.h"
#include "protos/egg.h"
#include <stddef.h>
#include <stdint.h>

typedef uint8_t stack[4096];
static stack stacks[10] = {};
void stivale2_main(struct stivale2_struct* info);
void kernel_main(struct eggHandle* egg_frame);
//extern void _start();

struct stivale2_header_tag_framebuffer framebuffer_request = {
	.tag = {
		.identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID,
		.next = 0},
	.framebuffer_width = 0,
	.framebuffer_height = 0,
	.framebuffer_bpp = 0,
};

__attribute__((section(".stivale2hdr"),used)) volatile struct stivale2_header header2 = {
	.entry_point = (uint64_t)stivale2_main,
	.stack = (uintptr_t)stacks[0] + sizeof(stack),
	.flags = 1 << 1,
	.tags = (uint64_t)&framebuffer_request};


static struct eggHandle eggFrame;


void stivale2_main(struct stivale2_struct* info)
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
				break;
			case STIVALE2_STRUCT_TAG_MEMMAP_ID:
			{
				struct stivale2_struct_tag_memmap* m = (struct stivale2_struct_tag_memmap*)tag;
				eggFrame.memMap.base = (struct eggMemoryMapEntry*)m->memmap;
				eggFrame.memMap.size = m->entries;
			}
			default:
				break;
		}

		tag = (struct stivale2_tag*)tag->next;
	}


	eggFrame.fb.fb_start = (void*)fbuff->framebuffer_addr;
	eggFrame.fb.fb_end = (void*)(fbuff->framebuffer_addr + fbuff->framebuffer_pitch * fbuff->framebuffer_height);
	eggFrame.fb.height = fbuff->framebuffer_height;
	eggFrame.fb.pitch = fbuff->framebuffer_pitch / 4;
	eggFrame.fb.width = fbuff->framebuffer_width; 

	kernel_main(&eggFrame);
}
