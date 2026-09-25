#include "limine.h"

#include "arch/addrspace.hpp"
#include "arch/archinfo.hpp"
#include "arch/cpu.hpp"
#include "gloxor/kinfo.hpp"
#include "gloxor/types.hpp"
#include "memory/alloc.hpp"
#include "memory/pmm.hpp"
#include "system/terminal.hpp"

using namespace gx;
using namespace arch;

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

inline void initialize_pmm(limine_memmap_response* m)
{
    const auto* mMap = m->entries;
    const auto entryCount = m->entry_count;
    for (size_t curIndex = 0; curIndex != entryCount; ++curIndex) {
        // for now reclaiming bootloader memory is too big of a risk as it can
        // corrupt page tables once memory manager can handle it, do it
        if (auto mTemp = mMap[curIndex]; mTemp->type == LIMINE_MEMMAP_USABLE) {
            gx::pmm_add_chunk(
                reinterpret_cast<void*>(mTemp->base + arch::physicalMemBase),
                mTemp->length
            );
        }
    }
}

inline BootInfo::MemTypes convert_mem_types(u32 type)
{
    using mtype = BootInfo::MemTypes;
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
        gloxUnreachable();
    }
}

inline void setup_kernelmemmap(limine_memmap_response* m)
{
    const auto* mMap = m->entries;
    const auto entryCount = m->entry_count;
    auto* memmap = (BootInfo::MemoryMap*)gx::PmmAllocator::alloc(
        sizeof(BootInfo::MemoryMap) * entryCount
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
    machineInfo.mmapEntries
        = glox::span<BootInfo::MemoryMap>(memmap, memmap + entryCount);
}

void limine_main()
{
    if (rsdp_request.response != nullptr)
        archInfo.acpiRsdp = (vaddr)rsdp_request.response->address;

    if (framebuffer_request.response != nullptr) {
        if (framebuffer_request.response->framebuffer_count >= 1) {
            auto fb = framebuffer_request.response->framebuffers[0];
            machineInfo.fbInfoEntry.begin = (vaddr)fb->address;
            machineInfo.fbInfoEntry.end
                = (uintptr_t)fb->address + fb->pitch * fb->height;
            machineInfo.fbInfoEntry.pitch = fb->pitch / 4;
            machineInfo.fbInfoEntry.height = fb->height;
            machineInfo.fbInfoEntry.width = fb->width;

            gx::term::init_term(
                (color_t*)fb->address,
                (color_t*)machineInfo.fbInfoEntry.end,
                machineInfo.fbInfoEntry.pitch,
                machineInfo.fbInfoEntry.width,
                machineInfo.fbInfoEntry.height
            );
        }
    }

    if (auto resp = exec_address_request.response; resp != nullptr) {
        kernelPhysOffset = resp->physical_base;
        kernelVirtOffset = resp->virtual_base;
        kernelMappingOffset = kernelPhysOffset - kernelVirtOffset;
    }

    if (auto resp = memmap_request.response; resp != nullptr) {
        initialize_pmm(resp);
        setup_kernelmemmap(resp);
    }

    machineInfo.kernelCode = { kernelFileBegin, kernelFileEnd };

    gloxor_main();
}
