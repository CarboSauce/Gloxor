export module arch_asm.pic8042contr;
import gloxor.types;

export namespace gx {
constexpr auto PIC1 = 0x20; /* IO base address for master PIC */
constexpr auto PIC2 = 0xA0; /* IO base address for slave PIC */
constexpr auto PIC1_COMMAND = PIC1; /*  */
constexpr auto PIC1_DATA = (PIC1 + 1); /*  */
constexpr auto PIC2_COMMAND = PIC2; /*  */
constexpr auto PIC2_DATA = (PIC2 + 1); /*  */
constexpr auto ICW1_ICW4 = 0x01; /* ICW4 (not) needed */
constexpr auto ICW1_SINGLE = 0x02; /* Single (cascade) mode */
constexpr auto ICW1_INTERVAL4 = 0x04; /* Call address interval 4 (8) */
constexpr auto ICW1_LEVEL = 0x08; /* Level triggered (edge) mode */
constexpr auto ICW1_INIT = 0x10; /* Initialization - required! */
constexpr auto ICW4_8086 = 0x01; /* 8086/88 (MCS-80/85) mode */
constexpr auto ICW4_AUTO = 0x02; /* Auto (normal) EOI */
constexpr auto ICW4_BUF_SLAVE = 0x08; /* Buffered mode/slave */
constexpr auto ICW4_BUF_MASTER = 0x0C; /* Buffered mode/master */
constexpr auto ICW4_SFNM = 0x10; /* Special fully nested (not) */
constexpr auto PIC_EOI = 0x20; /* End-of-interrupt ccommand code */
struct PicContext
{
    u8 masterMask;
    u8 slaveMask;
};
namespace pic {
    void set_master_mask(u8 mask);
    void set_slave_mask(u8 mask);
    void send_eoi_master();
    void send_eoi_slave();
    void remap(u8 masterOffst, u8 slaveOffst);
} // namespace pic
} // namespace gx
