import arch;
import glox;
import gloxor;
import gloxor.types;
import gloxor.system;
import gloxor.memory;
import std.compat;

#include "limine.h"
#include "glox/assert.hpp"

extern "C" void limine_main();
extern "C" void gloxor_main();

[[gnu::used, gnu::section(".limine_requests")]]
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

[[gnu::used, gnu::section(".limine_requests_start")]]
static volatile std::uint64_t limine_requests_start_marker[]
    = LIMINE_REQUESTS_START_MARKER;

[[gnu::used, gnu::section(".limine_requests_end")]]
static volatile std::uint64_t limine_requests_end_marker[]
    = LIMINE_REQUESTS_END_MARKER;

[[gnu::used, gnu::section(".limine_requests")]]
volatile limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

[[gnu::used, gnu::section(".limine_requests")]]
volatile limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

[[gnu::used, gnu::section(".limine_requests")]]
volatile limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

[[gnu::used, gnu::section(".limine_requests")]]
volatile limine_executable_address_request exec_address_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

[[gnu::used, gnu::section(".limine_requests")]]
volatile limine_stack_size_request stack_size_request = {
    .id = LIMINE_STACK_SIZE_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
    .stack_size = 8192,
};

inline void initialize_pmm(limine_memmap_response* m)
{
    const auto* mMap = m->entries;
    const auto entryCount = m->entry_count;
    for (size_t curIndex = 0; curIndex != entryCount; ++curIndex) {
        // for now reclaiming bootloader memory is too big of a risk as it can
        // corrupt page tables once memory manager can handle it, do it
        if (auto mTemp = mMap[curIndex]; mTemp->type == LIMINE_MEMMAP_USABLE) {
            gx::boot_add_chunk(
                reinterpret_cast<void*>(mTemp->base + arch::PHYSICAL_MEM_BASE),
                mTemp->length
            );
        }
    }
}

inline gx::BootInfo::MemTypes convert_mem_types(u32 type)
{
    using mtype = gx::BootInfo::MemTypes;
    switch (type) {
    case LIMINE_MEMMAP_USABLE:
        return mtype::usable;
    case LIMINE_MEMMAP_RESERVED:
        return mtype::reserved;
    case LIMINE_MEMMAP_ACPI_NVS:
        return mtype::acpiNvs;
    case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
        return mtype::acpiReclaim;
    case LIMINE_MEMMAP_BAD_MEMORY:
        return mtype::badMem;
    case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
        return mtype::reclaimable;
    case LIMINE_MEMMAP_FRAMEBUFFER:
        return mtype::framebuffer;
    case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
        return mtype::kernel;
    case LIMINE_MEMMAP_RESERVED_MAPPED:
        return mtype::reserved;
    default:
        GLOX_UNREACHABLE();
    }
    GLOX_UNREACHABLE();
}

inline void setup_kernelmemmap(limine_memmap_response* m)
{
    const auto* mMap = m->entries;
    const auto entryCount = m->entry_count;
    auto* memmap = (gx::BootInfo::MemoryMap*)gx::PmmAllocator::alloc(
        sizeof(gx::BootInfo::MemoryMap) * entryCount
    );
    glox::uninit_def_construct(memmap, memmap + entryCount);

    for (size_t i = 0; i != entryCount; ++i) {
        auto mTemp = mMap[i];
        memmap[i] = {
            .base = mTemp->base,
            .length = mTemp->length,
            .type = convert_mem_types(mTemp->type),
        };
    }
    gx::machineInfo.mmapEntries
        = glox::span<gx::BootInfo::MemoryMap>(memmap, memmap + entryCount);
}

void limine_main()
{
    if (auto resp = memmap_request.response; resp != nullptr) {
        initialize_pmm(resp);
        setup_kernelmemmap(resp);
    }

    if (rsdp_request.response != nullptr)
        arch::archInfo.acpiRsdp = (vaddr)rsdp_request.response->address;

    if (framebuffer_request.response != nullptr) {
        if (framebuffer_request.response->framebuffer_count >= 1) {
            auto fb = framebuffer_request.response->framebuffers[0];
            gx::machineInfo.fbInfoEntry.begin = (vaddr)fb->address;
            gx::machineInfo.fbInfoEntry.end
                = (uintptr_t)fb->address + fb->pitch * fb->height;
            gx::machineInfo.fbInfoEntry.pitch = fb->pitch;
            gx::machineInfo.fbInfoEntry.height = fb->height;
            gx::machineInfo.fbInfoEntry.width = fb->width;
            gx::machineInfo.fbInfoEntry.redMaskSize = fb->red_mask_size;
            gx::machineInfo.fbInfoEntry.redMaskShift = fb->red_mask_shift;
            gx::machineInfo.fbInfoEntry.greenMaskSize = fb->green_mask_size;
            gx::machineInfo.fbInfoEntry.greenMaskShift = fb->green_mask_shift;
            gx::machineInfo.fbInfoEntry.blueMaskSize = fb->blue_mask_size;
            gx::machineInfo.fbInfoEntry.blueMaskShift = fb->blue_mask_shift;
        }
    }

    if (auto resp = exec_address_request.response; resp != nullptr) {
        arch::kernelPhysOffset = resp->physical_base;
        arch::kernelVirtOffset = resp->virtual_base;
        arch::kernelMappingOffset
            = arch::kernelPhysOffset - arch::kernelVirtOffset;
    }

    gx::machineInfo.kernelCode = { kernelFileBegin, kernelFileEnd };
    gx::machineInfo.mainStackTop = (uintptr_t)gx::page_alloc(2);
    gx::machineInfo.mainStackSize = 4096 * 2;
    arch::run_on_stack(gx::machineInfo.mainStackTop, gloxor_main);
}
