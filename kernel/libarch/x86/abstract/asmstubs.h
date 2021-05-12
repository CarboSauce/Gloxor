#pragma once
#include <cstdint>


inline void spinloop_hint ()
{
   asm("pause");
}

inline void halt ()
{
   asm("hlt");
}

inline void stop_interrupts ()
{
   asm volatile("cli" ::: "memory");
}

inline void start_interrupts ()
{
   asm volatile("sti" ::: "memory");
}

inline void outb (uint16_t port, uint8_t val)
{
   __asm__ volatile("outb %0, %1"
                    :
                    : "a"(val), "Nd"(port));
   /* There's an outb %al, $imm8  encoding, for compile-time constant port numbers that fit in 8b.  (N constraint).
     * Wider immediate constants would be truncated at assemble-time (e.g. "i" constraint).
     * The  outb  %al, %dx  encoding is the only option for all other cases.
     * %1 expands to %dx because  port  is a uint16_t.  %w1 could be used if we had the port number a wider C type */
}

inline uint8_t inb (uint16_t port)
{
   uint8_t ret;
   __asm__ volatile("inb %1, %0"
                    : "=a"(ret)
                    : "Nd"(port));
   return ret;
}
