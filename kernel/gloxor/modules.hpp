#pragma once

namespace gx {
using module_t = void (*)(void);
}

#define _registerModule(fnc, type, priority) [[maybe_unused, gnu::used, gnu::section(".module." #type "." #priority)]] static gx::module_t _moduleptr_##fnc##_##type = &(fnc)

#define initPreCpuModule(fnc) _registerModule(fnc, precpu, 0)
#define initDriverCentralModule(fnc) _registerModule(fnc, driver, 0)
#define initDriverModule(fnc) _registerModule(fnc, driver, 1)
