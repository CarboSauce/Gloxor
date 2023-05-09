#include "limine.h"
#include "arch/addrspace.hpp"
#include "arch/archinfo.hpp"
#include "gloxor/kinfo.hpp"
#include "gloxor/types.hpp"
#include "memory/pmm.hpp"

using namespace gx;
using namespace arch;

extern "C" void limine_main();
extern "C" void gloxor_main();

static limine_rsdp_request rsdp_req     = { LIMINE_RSDP_REQUEST, 0, nullptr };
static limine_memmap_request memmap_req = { LIMINE_MEMMAP_REQUEST, 0, nullptr };
static limine_framebuffer_request fb_req
	= { LIMINE_FRAMEBUFFER_REQUEST, 0, nullptr };
static limine_hhdm_request hhdm_req = { LIMINE_HHDM_REQUEST, 0, nullptr };
static limine_kernel_address_request addr_req
	= { LIMINE_KERNEL_ADDRESS_REQUEST, 0, nullptr };
[[gnu::section(".limine_reqs"), gnu::used]] static void* _limine_reqs[]
	= { (void*)&addr_req,
		  (void*)&hhdm_req,
		  (void*)&fb_req,
		  (void*)&rsdp_req,
		  (void*)&memmap_req,
		  nullptr };

void limine_main()
{
	if (rsdp_req.response != nullptr)
		archInfo.acpiRsdp = (paddrT)rsdp_req.response->address;

	gloxor_main();
}
