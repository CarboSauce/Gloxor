#pragma once

#if __STDC_HOSTED__
   #include <cstring>
   #define GLOX_STRLEN(x) std::strlen(x)
#else
   // In freestanding we assume that user implements string.h
   #include <string.h>
   #define GLOX_STRLEN(x) strlen(x)
#endif