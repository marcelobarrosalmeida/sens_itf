#ifndef __OS_KERNEL__
#define __OS_KERNEL__ 

#ifdef __cplusplus
extern "C" {
#endif

struct os_thread_s;
typedef struct os_thread_s * os_thread_t;
typedef void* (*os_kernel_func)(void *);
typedef void* os_thread_arg;

/**
    Suspend the current thread for time_ms milliseconds.
    @param time_ms Number of milliseconds to sleep.
*/

void os_kernel_sleep(uint32_t time_ms);
int os_kernel_get_def_pri(void);
unsigned int os_kernel_get_def_stack(void);
unsigned int os_kernel_get_def_time_slice(void);
os_thread_t os_kernel_create(os_kernel_func entry_point, const char *task_name, os_thread_arg arg, int pri, int stack_size, int time_slice_ms, int auto_start);

#ifdef __cplusplus
}
#endif

#endif /*  __OS_KERNEL__ */


