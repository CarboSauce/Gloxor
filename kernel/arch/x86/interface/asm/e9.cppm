export module arch_asm:e9;
import asm : asmstubs;
import gloxor : mutex;
import gloxor : types;

inline void print_e9(const char* str)
{
    glox::scoped_lock<gx::IrqLock> _;
    while (char c = *str++) {
        outb(0xE9, c);
    }
}

inline void print_e9(const char* str, size_t size)
{
    glox::scoped_lock<gx::IrqLock> _;
    for (size_t i = 0; i != size; ++i) {
        outb(0xE9, str[i]);
    }
}
