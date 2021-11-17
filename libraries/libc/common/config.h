#ifndef SMOL_CONFIG_MACROS_H_
#define SMOL_CONFIG_MACROS_H_

#ifdef __cplusplus

   #ifndef SMOL_INLINE
      #define SMOL_INLINE inline
   #endif

   #ifndef SMOL_NOEXCEPT
      #define SMOL_NOEXCEPT(x) noexcept(x)
   #endif

   #define BEGIN_EXTERN_C \
      extern "C"          \
      {
   #define END_EXTERN_C }

#else

   #ifndef SMOL_INLINE
      #ifdef __GNU__
         #define SMOL_INLINE static __inline__ __cdecl \
             __attribute__ ((__always_inline__, __gnu_inline__))
      #else
         #define SMOL_INLINE static inline
      #endif
   #endif

   #ifndef SMOL_NOEXCEPT
      #define SMOL_NOEXCEPT(x)
   #endif

   #define BEGIN_EXTERN_C // empty macro
   #define END_EXTERN_C   // empty macro

#endif

#endif //End Header Guard