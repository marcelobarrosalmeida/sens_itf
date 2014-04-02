#ifndef __OS_KERNEL__
#define __OS_KERNEL__ 

#ifdef __cplusplus
extern "C" {
#endif

/**
    Suspend the current thread for time_ms milliseconds.
    @param time_ms Number of milliseconds to sleep.
*/

extern void os_kernel_sleep(uint32_t time_ms);

#ifdef __cplusplus
}
#endif

#endif /*  __OS_KERNEL__ */


