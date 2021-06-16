#pragma once
#include <stdint.h>

namespace statusFlags
{ // clang-format off
   enum eflags
   { 
      carry       = 0x00000001,
      parity      = 0x00000004,
      adjust      = 0x00000010,
      zero        = 0x00000040,
      sign        = 0x00000080, 
      trap        = 0x00000100, // set if cpu is in single step
      interrupt   = 0x00000200,
      direction   = 0x00000400,
      overflow    = 0x00000800,
      iopl        = 0x00003000, // IO privilege flag
      resume      = 0x00010000,
      vm          = 0x00020000, // Virtual 8086 mode flag
      ac          = 0x00040000, // Allignment check
      vif         = 0x00080000, // Virtual interrupt flag
      vip         = 0x00100000, // Virtual interrupt pending
      cpuid       = 0x00200000, // if able to use CPUID
   };
} // clang-format on

inline void ioWait()
{
   /* Port 0x80 is used for 'checkpoints' during POST. */
   /* The Linux kernel seems to think it is free for use :-/ */
   __asm__ volatile("outb %%al, $0x80"
                    :
                    : "a"(0));
   /* %%al instead of %0 makes no difference.  TODO: does the register need to be zeroed? */
}

inline void outb(uint16_t port, uint8_t val)
{
   __asm__ volatile("outb %0, %1"
                    :
                    : "a"(val), "Nd"(port));
   /* There's an outb %al, $imm8  encoding, for compile-time constant port numbers that fit in 8b.  (N constraint).
     * Wider immediate constants would be truncated at assemble-time (e.g. "i" constraint).
     * The  outb  %al, %dx  encoding is the only option for all other cases.
     * %1 expands to %dx because  port  is a uint16_t.  %w1 could be used if we had the port number a wider C type */
}

inline uint8_t inb(uint16_t port)
{
   uint8_t ret;
   __asm__ volatile("inb %1, %0"
                    : "=a"(ret)
                    : "Nd"(port));
   return ret;
}

inline statusFlags::eflags getStatusFlags()
{
   statusFlags::eflags flags;
   __asm__ volatile("pushf;pop %0"
                    : "=r"(flags)
                    :
                    : "memory");
   return flags;
}

inline void setStatusFlags(statusFlags::eflags flags)
{
   __asm__ volatile("push %0;popf"
                    :
                    : "rm"(flags)
                    : "memory", "cc");
}