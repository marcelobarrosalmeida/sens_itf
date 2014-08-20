#include <windows.h>
#include <stdint.h>
#include "os_kernel.h"
#include "os_util.h"

#define OS_DBG_SO_THREAD  0
#define OS_DBG_SO_SEMA    0
#define OS_DBG_SO_CRITSEC 0

#define OS_THREAD_MAX_NAME 16

const int OS_THREAD_DEFAULT_PRI = 0;
const int OS_THREAD_DEFAULT_STACK = 120;

typedef unsigned int(__stdcall *win32_thead_func)(void *);

struct os_thread_s
{
    HANDLE handle;
    win32_thead_func entry_point;
    os_thread_arg arg;
    int pri;
    int stack_size;
    char name[OS_THREAD_MAX_NAME];
};

void os_kernel_sleep(uint32_t time_ms)
{
	Sleep(time_ms);
}

int os_kernel_get_def_pri(void)
{
    return OS_THREAD_DEFAULT_PRI;
}

unsigned int os_kernel_get_def_stack(void)
{
    return OS_THREAD_DEFAULT_STACK;
}

unsigned int os_kernel_get_def_time_slice(void)
{
    return 0;
}


os_thread_t os_kernel_create(os_kernel_func entry_point, const char* task_name, os_thread_arg arg,
    int pri, int stack_size, int time_slice_ms, int auto_start)
{
    os_thread_t tsk = NULL;
    /* time_slice and auto_start not used for win32 port */

    OS_UTIL_LOG(OS_DBG_SO_THREAD, ("Creating thread: %s, priority: %d, stack_size: %d - Allocating TCB...", task_name, pri, stack_size));


    tsk = (os_thread_t) calloc(1, sizeof(struct os_thread_s));
    OS_UTIL_ASSERT(tsk);

    tsk->entry_point = (win32_thead_func) entry_point;
    tsk->arg = arg;
    tsk->pri = pri;
    tsk->stack_size = stack_size;
    strncpy(tsk->name, task_name, OS_THREAD_MAX_NAME);

    OS_UTIL_LOG(OS_DBG_SO_THREAD, ("Creating thread (EP=%08X, ARGS=%08X, PRI=%d)",
        (void *) entry_point, arg, pri));

    // security attributes
    // stack size
    // thread's start address
    // arguments
    // thread's initial state
    // return thread id.

    tsk->handle = (HANDLE) _beginthreadex(NULL, 0, tsk->entry_point, tsk->arg,
        auto_start ? 0 : CREATE_SUSPENDED, NULL);

    OS_UTIL_ASSERT(tsk->handle);

    OS_UTIL_LOG(OS_DBG_SO_THREAD, ("Thread created"));

    return tsk;
}
