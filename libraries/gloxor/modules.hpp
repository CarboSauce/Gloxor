#pragma once


namespace glox
{
   using module_t = void(*)(void);
}

#define _registerModule(fnc,type) [[maybe_unused, gnu::used, gnu::section(".module."#type)]] \
static glox::module_t _moduleptr_##fnc##_##type  = &fnc 

#define initPreCpuModule(fnc) _registerModule(fnc,precpu)
#define initDriverCentralModule(fnc) _registerModule(fnc,central)
#define initDriverModule(fnc) _registerModule(fnc,driver)

#ifdef GLOXTESTING
   #define registerTest(fnc) _registerModule(fnc,testing)
#else
   #define registerTest(fnc) 
#endif