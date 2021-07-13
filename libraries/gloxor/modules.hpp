#pragma once


namespace glox
{
   using module_t = void(*)(void);
}

#define _registerModule(fnc,type) [[maybe_unused, gnu::used, gnu::section(".module."#type)]] \
static glox::module_t _moduleptr_##fnc##_##type  = &fnc 


// for now its total hack, clang seems to aggresively delete unused functions
// which ends in deleting init functions
#define initFunc [[gnu::used]] 

#define initPreCpuModule(fnc) [[maybe_unused, gnu::used, gnu::section(".module.precpu")]] \
static glox::module_t _moduleptr_##fnc  = &fnc 
#define initDriverCentralModule(fnc) [[maybe_unused, gnu::used, gnu::section(".module.central")]] \
static glox::module_t _moduleptr_##fnc  = &fnc 
#define initDriverModule(fnc) [[maybe_unused, gnu::used, gnu::section(".module.driver")]] \
static glox::module_t _moduleptr_##fnc  = &fnc 
