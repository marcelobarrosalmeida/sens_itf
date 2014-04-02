#include <Windows.h>
#include <stdint.h>
#include "os_defs.h"
#include "os_timer.h"
#include "../sens_itf/sens_util.h"



#define OS_ASSERT SENS_UTIL_ASSERT

enum {
    WIN_TIMER_OFF = 0,
    WIN_TIMER_ON
};

struct os_timer_s
{
    HANDLE handle;
    os_timer_func   timer_func;
    uint32_t    expire_in_ms;
    uint32_t    reschedule_in_ms;
    os_timer_type_t time_type;
    void*           timer_arg;
    int             state;
};

static VOID CALLBACK timer_callback(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
    os_timer_t tmr = (os_timer_t)lpParameter;
    tmr->timer_func(tmr->timer_arg);
}

static int add_timer(os_timer_t tmr, uint32_t expire_in_ms, uint32_t reschedule_in_ms)
{
    int status;

    tmr->reschedule_in_ms = reschedule_in_ms;
    tmr->expire_in_ms = expire_in_ms;
    tmr->time_type = reschedule_in_ms > 0 ? OS_TIMER_CYCLIC : OS_TIMER_ONE_SHOT;

    tmr->handle = CreateWaitableTimer(NULL, TRUE, "");
    OS_ASSERT(tmr->handle);

    status = CreateTimerQueueTimer(&tmr->handle, NULL, timer_callback, (void *)tmr,
        tmr->expire_in_ms, tmr->reschedule_in_ms,
        WT_EXECUTEDEFAULT);
    OS_ASSERT(status);

    tmr->state = WIN_TIMER_ON;

    return (status ? OS_SUCCESS : OS_ERROR);
}

static void del_timer(os_timer_t tmr)
{
    if (tmr->state == WIN_TIMER_ON)
    {
        int status;
        status = DeleteTimerQueueTimer(NULL, tmr->handle, NULL);
        OS_ASSERT(status);
        //CloseHandle(tmr->handle);
    }
}

os_timer_t os_timer_create(os_timer_func timer_func, void *timer_arg,
    uint32_t expire_in_ms, uint32_t reschedule_in_ms,
    uint32_t auto_start)
{
    os_timer_t tmr = NULL;
    tmr = (os_timer_t)calloc(1, sizeof(struct os_timer_s));
    OS_ASSERT(tmr);

    tmr->timer_func = timer_func;
    tmr->timer_arg = timer_arg;
    tmr->state = WIN_TIMER_OFF;

    if (auto_start)
        add_timer(tmr, expire_in_ms, reschedule_in_ms);

    return tmr;
}

uint32_t os_timer_change(os_timer_t tmr, uint32_t expire_in_ms, uint32_t reschedule_in_ms)
{
    OS_ASSERT(tmr);

    del_timer(tmr);

    return add_timer(tmr, expire_in_ms, reschedule_in_ms);
}

uint32_t os_timer_activate(os_timer_t tmr)
{
    OS_ASSERT(tmr);

    del_timer(tmr);

    return add_timer(tmr, tmr->expire_in_ms, tmr->reschedule_in_ms);
}

uint32_t os_timer_deactivate(os_timer_t tmr)
{
    OS_ASSERT(tmr);

    del_timer(tmr);
    tmr->state = WIN_TIMER_OFF;

    return OS_SUCCESS;
}

uint32_t os_timer_delete(os_timer_t tmr)
{
    OS_ASSERT(tmr);

    del_timer(tmr);
    tmr->state = WIN_TIMER_OFF;
    free(tmr);

    return OS_SUCCESS;
}
