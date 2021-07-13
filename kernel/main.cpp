#include "arch/cpu.hpp"
#include "arch/irq.hpp"
#include "glox/algo.hpp"
#include "gloxor/graphics.hpp"
#include "protos/egg.h"
#include "system/danger.hpp"
#include "system/logging.hpp"
#include "string.h"

using ctor_t = void (*)(void);
using namespace arch;

extern ctor_t _ctorArrayStart[];
extern ctor_t _ctorArrayEnd[];
extern ctor_t _modulePreCpuBegin[];
extern ctor_t _moduleDriverCentralBegin[];
extern ctor_t _moduleDriverEnd[];

extern "C" void callCtorPointers(ctor_t* begin, ctor_t* end)
{

	gloxLog("There are: ", static_cast<glox::u32>(end - begin), " Ctors\n");
	for (auto it = begin; it != end; ++it)
	{
		(*it)();
	}
}

extern "C" void callPreCpuInits()
{

	gloxLogln ("Pre Cpu Init:");

	callCtorPointers(_modulePreCpuBegin, _moduleDriverCentralBegin);
}

extern "C" void callDriverInits()
{
	gloxLogln ("Driver Init:");	
	callCtorPointers(_moduleDriverCentralBegin, _moduleDriverEnd);
	// We assume that Drivers havent enabled interrupts hopefully
	gloxDebugLog("Starting Interrupts after driver initialization\n");
	arch::startIrq();
}

extern "C" void callGlobalCtors()
{
	gloxLogln ("Global ctors :");
	callCtorPointers(_ctorArrayStart, _ctorArrayEnd);
}


void gogole_test(eggHandle*);
extern void sleep(u64 ticks, u64 ms);
extern u64 getTicks();
glox::framebuffer con;
  
extern "C" void kernel_main(eggHandle* eggFrame)
{
	gloxLogln("Really weird but: ", strlen("Pepega"));


	callPreCpuInits();
	initializeCpu();
 
	callDriverInits();
	callGlobalCtors();
	gogole_test(eggFrame);
	


	// Invoking strlen doesnt work if strlen is a function defined as __builtin_strlen

	u64 ticker = 0;
	char buff[30];
	while(1)
	{
		auto _tik = getTicks();
		buff[glox::format(buff,ticker)+1] = '\0';
		con.writeString(buff);
		con.curX = 0;
		con.curY = 0;
		sleep(_tik,1000);
		ticker+=1;
	}
}


void logFrameBuffer()
{
	gloxLogln("Frame Buffer Begin: ", con.fbBeg);
	gloxLogln("Frame Buffer End:", con.fbEnd);
	gloxLogln("Frame Buffer Height", con.height);
	gloxLogln("Frame Buffer Width", con.width);
	gloxLogln("Frame Buffer Pitch", con.pitch);
}

// Super useless and unscientific tests, take with grain of salt
void gogole_test(eggHandle* eggFrame)
{
	
	con = {

		(glox::rgb_t*)eggFrame->fb.fb_start,
		(glox::rgb_t*)eggFrame->fb.fb_end,
		eggFrame->fb.pitch,
		eggFrame->fb.width,
		eggFrame->fb.height,
		0xFFFFFF //white color
	};
	con.cls(0x101010);


	logFrameBuffer();
	gloxLogln("Memory map moment");
	for (size_t i = 0; i < eggFrame->memMap.size; ++i)
	{
		gloxLogln("From ",(void*)eggFrame->memMap.base[i].base," to ",
		 (void*)(eggFrame->memMap.base[i].base+eggFrame->memMap.base[i].length),
		 " Value = ", eggFrame->memMap.base[i].type);
		
	}


}
