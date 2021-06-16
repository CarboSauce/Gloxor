#include "debug.hpp"
#include "glox/algo.hpp"
#include "glox/graphics.hpp"
#include "glox/logger.hpp"
#include "arch/cpu.hpp"
#include "protos/egg.h"
#include "string.h"
#include <cstdint>
#include "glox/algo.hpp"
#include "danger.hpp"

using global_ctor_t = void (*)(void);
using namespace arch;

extern global_ctor_t _ctor_array_start[];
extern global_ctor_t _ctor_array_end[];

extern "C" void call_global_ctors()
{
   print_debug("There are: ");
   glox::log_integer(print_debug,(uint32_t)(_ctor_array_end-_ctor_array_start));
   print_debug(" Ctors\n");

   for (auto it = _ctor_array_start; it != _ctor_array_end; ++it)
   {
      (*it)();
   }
}

void gogole_test(egg_t*);

extern "C" void kernel_main(egg_t* egg_frame)
{
   initializeCpu();
   call_global_ctors();

   gogole_test(egg_frame);
}

glox::framebuffer con;

void logFrameBuffer()
{
   print_debug("\n\nFrame Buffer Begin: ");
   glox::log_pointer(print_debug,con.fbBeg);
   print_debug("\nFrame Buffer End:");
      glox::log_pointer(print_debug,con.fbEnd);
   print_debug("\nFrame Buffer Height");
      glox::log_integer(print_debug,con.height);
   print_debug("\nFrame Buffer Width");
      glox::log_integer(print_debug,con.width);
   print_debug("\nFrame Buffer Pitch");
      glox::log_integer(print_debug,con.pitch);
   print_debug("\n\n\n");
}

// Super useless and unscientific tests, take with grain of salt
void gogole_test(egg_t* egg_frame)
{
   con = {
       (glox::rgb_t*)egg_frame->fb.fb_start,
       (glox::rgb_t*)egg_frame->fb.fb_end,
       egg_frame->fb.pitch,
       egg_frame->fb.width,
       egg_frame->fb.height,
       0xFFFFFF //white color
   };

   logFrameBuffer();

   
   glox::kernelPanic();

   print_debug("RULES OF NATURE!\n");
   while (1)
      ;
}
