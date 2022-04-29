#pragma once

namespace arch
{

   inline void spinloopHint()
   {
      asm("pause");
   }

   inline void halt()
   {
      asm("hlt");
   }

   void initializeCpu();

   [[noreturn]] inline void haltForever()
   {
	   while (1)
	   {
		   asm("cli;hlt");
	   }
   }


} // namespace arch
