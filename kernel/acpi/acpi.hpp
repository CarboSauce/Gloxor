#pragma once
#include "gloxor/types.hpp"

namespace acpi
{
struct [[gnu::packed]] rsdp
{
	char signature[8];
	u8 checksum;
	char oemid[6];
	u8 revision;
	u32 rsdtAddr;
};
struct [[gnu::packed]] xsdp : rsdp
{
	u32 len;
	u64 xsdtAddr;
	u8 exChecksum;
	u8 reserved[3];
};
struct [[gnu::packed]] sdtHeader
{
	char signature[4];
	u32 len;
	u8 revision;
	u8 checksum;
	char oemid[6];
	char oemTableid[8];
	u32 oemRevision;
	u32 creatorid;
	u32 creatorRevision;
};
struct [[gnu::packed]] rsdt : sdtHeader
{
	u32 sdtTable[];
};
struct [[gnu::packed]] madt : sdtHeader
{
	constexpr static u32 magic = 0x41435049; // ACPI
	u32 lapicAddr;
	u32 flags;
	struct [[gnu::packed]] entry
	{
		u8 type;
		u8 len;
	} entries[];
	enum class type : u8
	{
		lapic = 0,
		ioapic = 1,
		iso = 2,
		nmi = 4,
		lapicOverride = 5,
		x2lapic = 9
	};
	struct [[gnu::packed]] lapic : entry
	{
		u8 processorId;
		u8 acpiId;
		u32 flags;
	};
	struct [[gnu::packed]] ioapic : entry
	{
		u8 ioapicId;
		u8 reserved;
		u32 ioapicAddr;
		u32 interruptBase;
	};
	struct [[gnu::packed]] ioapicOverride : entry
	{
		u8 bus;
		u8 irq;
		u32 sysInterrupt;
		u16 flags;
	};
	struct [[gnu::packed]] ioapicNmi : entry
	{
		u8 nmi;
		u8 reserved;
		u16 flags;
		u32 sysInterrupt;
	};
	struct [[gnu::packed]] lapicNmi : entry
	{
		u8 id;
		u16 flags;
		u8 lint;
	};
	struct [[gnu::packed]] lapicOverride : entry
	{
		u16 reserved;
		u64 lapicAddr;
	};
	struct [[gnu::packed]] x2lapic : entry
	{
		u16 reserved;
		u32 id;
		u32 flags;
		u32 acpiId;
	};
	struct [[gnu::packed]] mcfg : sdtHeader
	{
		constexpr static u32 magic = 0x4d434647; // MCFG
		u64 reserved;
		struct entry
		{
			u64 addr;
			u16 pciSegGroup;
			u8 busStart;
			u8 busEnd;
			u32 reserved;
		} entires[];
	};
	struct [[gnu::packed]] hpet : sdtHeader
	{
		constexpr static u32 magic = 0x48504554; // HPET
		u8 hwRevId;
		u8 info;
		u16 pciVendorId;
		u8 addrSpaceId;
		u8 regBitWidth;
		u8 regBitOffset;
		u8 reserved;
		u64 addr;
		u8 hpetNumber;
		u16 minTick;
		u8 pageProt;
	};
};
} // namespace acpi