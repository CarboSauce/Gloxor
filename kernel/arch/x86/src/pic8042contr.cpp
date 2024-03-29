#include "asm/pic8042contr.hpp"
#include "asm/asmstubs.hpp"
#include "gloxor/modules.hpp"
#include "system/logging.hpp"
// everything is masked on startup
static gx::PicContext picCtx { 0xFF, 0xFF };

namespace gx::pic {
void send_eoi_slave()
{
	outb(PIC2_COMMAND, PIC_EOI);
}
void send_eoi_master()
{
	outb(PIC1_COMMAND, PIC_EOI);
}

void remap(u8 offset1, u8 offset2)
{
	unsigned char a1, a2;

	a1 = inb(PIC1_DATA); // save masks
	a2 = inb(PIC2_DATA);

	outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4); // starts the initialization sequence (in cascade mode)
	io_wait();
	outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
	io_wait();
	outb(PIC1_DATA, offset1); // ICW2: Master PIC vector offset
	io_wait();
	outb(PIC2_DATA, offset2); // ICW2: Slave PIC vector offset
	io_wait();
	outb(PIC1_DATA, 4); // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
	io_wait();
	outb(PIC2_DATA, 2); // ICW3: tell Slave PIC its cascade identity (0000 0010)
	io_wait();

	outb(PIC1_DATA, ICW4_8086);
	io_wait();
	outb(PIC2_DATA, ICW4_8086);
	io_wait();
	outb(PIC1_DATA, a1); // restore saved masks.
	outb(PIC2_DATA, a2);
}

void set_master_mask(u8 mask)
{
	picCtx.masterMask &= mask;
	outb(PIC1_DATA, picCtx.masterMask);
}
void set_slave_mask(u8 mask)
{
	picCtx.slaveMask &= mask;
	outb(PIC1_DATA, picCtx.slaveMask);
}
} // namespace gx::pic

static void init_pic()
{
	gx::pic::remap(0x20, 0x28);
	gloxDebugLog("Initializing PIC\n");
	// Mask all devices
	outb(PIC1_DATA, 0xFF);
	outb(PIC2_DATA, 0xFF);
}

initDriverCentralModule(init_pic);
