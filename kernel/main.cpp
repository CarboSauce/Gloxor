#include "graphics/shell.hpp"
#include "libarch/cpu.hpp"
#include "libglox/logger.hpp"
#include "libsane/debug.hpp"
#include "protos/egg.h"
#include <cstdint>

//TEMP
#include "libarch/x86/idt.hpp"

using global_ctor_t = void (*) (void);
using namespace arch;

extern global_ctor_t _ctor_array_start[];
extern global_ctor_t _ctor_array_end[];


static __attribute__ ((interrupt)) void GPfault(interrupt_frame_t* frame,size_t errc);
static __attribute__ ((interrupt)) void DoubleFault (interrupt_frame_t* frame);
static __attribute__ ((interrupt)) void DivZeroHandle (interrupt_frame_t* frame);
static __attribute__ ((interrupt)) void SpurInterrupt (interrupt_frame_t* frame);



extern "C" void call_global_ctors ()
{
   for (auto it = _ctor_array_start; it < _ctor_array_end; ++it)
   {
      (*it) ();
   }
}

void gogole_test (egg_t*);

extern "C" void kernel_main (egg_t* egg_frame)
{

   call_global_ctors ();
   initialize_cpu ();

   gogole_test (egg_frame);
}


sane::shell con;
// Super useless and unscientific tests, take with grain of salt
void gogole_test (egg_t* egg_frame)
{
   con = {
      (sane::rgb_t*)egg_frame->fb.fb_start,
      (sane::rgb_t*)egg_frame->fb.fb_end,
      egg_frame->fb.pitch,
      egg_frame->fb.height,
      0xFFFFFF //white color
   };

   auto log_ptr = [] (void* ptr) {
      glox::log_pointer (print_debug, ptr);
   };

   auto log_int = [] (uint32_t big) {
      glox::log_integer ([] (const char* ptr) {
         con.writeString (ptr);
      },
                         big);
   };

   con.cls (0x0);

   print_debug ("\n");


   idt* idt_list = (idt*)arch::get_idt();

   while (1)
      ;
}


