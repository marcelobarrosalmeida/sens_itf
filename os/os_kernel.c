#include <windows.h>
#include <stdint.h>
#include "os_kernel.h"

void os_kernel_sleep(uint32_t time_ms)
{
	Sleep(time_ms);
}

