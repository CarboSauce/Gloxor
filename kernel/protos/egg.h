#pragma once
#include <stddef.h>
#include <stdint.h>

#define EGG_MEM_USABLE                 1
#define EGG_MEM_RESERVED               2
#define EGG_MEM_ACPI_RECLAIMABLE       3
#define EGG_MEM_ACPI_NVS               4
#define EGG_MEM_BAD_MEMORY             5
#define EGG_MEM_FRAMEBUFFER            0x1002
#define EGG_MEM_RECLAIMABLE            0x1000
#define EGG_MEM_KERNEL_AND_MODULES     0x1001

struct eggFramebuffer
{
	void* fb_start;
	void* fb_end;
	size_t pitch;
	size_t height;
	size_t width;  
};


struct eggMemoryMapEntry
{
	uint64_t base;
	uint64_t length;
	uint32_t type;
	uint32_t unused;
};

struct eggMemoryMap 
{
	struct eggMemoryMapEntry* base;
	uint64_t size;
};

struct eggHandle {
	struct eggFramebuffer fb;
	struct eggMemoryMap memMap;
};