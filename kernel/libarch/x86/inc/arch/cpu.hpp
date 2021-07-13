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

   void haltForever();
} // namespace arch
