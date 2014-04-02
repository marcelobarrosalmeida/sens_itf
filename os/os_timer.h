#ifndef __OS_TIMER_H__
#define __OS_TIMER_H__

#ifdef __cplusplus
extern "C" {
#endif 

typedef enum os_timer_type_e
{
    OS_TIMER_ONE_SHOT = 0,
    OS_TIMER_CYCLIC
} os_timer_type_t;

/** 
 * Timer handler
 * */
typedef struct os_timer_s *os_timer_t;

/**
 * timer callback
 * */
typedef void (*os_timer_func)(void *);

/**
 * Initialize timer infrastructure.
 * This routine must be called before any timer usage.
 */
void os_timer_init(void);

/**
 * Creates a new timer.
 * 
 * @param callback      callback called when the timer expires
 * @param arg           user defined argument used in callback
 * @param schedule_ms   first expiration time, in ms
 * @param reschedule_ms if you want a cyclic timer, use this parameter
 *                      to specify further expirations (ms)
 * @param enabled       Use 1 to create an active timer and 0 to create
 *                      an inactive timer

 * @retval a valid timer handler or null pointer
 */
os_timer_t os_timer_create(os_timer_func callback, 
                           void *arg, 
                           uint32_t schedule_ms, 
                           uint32_t reschedule_ms, 
                           uint32_t enabled);

/**
 * Delete a timer
 * 
 * @param tmr timer handler
 * @retval OS_SUCCESS timer deleted
 * @retval OS_ERROR   error when deleting a timer
 */
uint32_t os_timer_delete(os_timer_t tmr);

/**
 * Activate a timer
 * 
 * @param tmr timer handler
 * @retval OS_SUCCESS timer activated
 * @retval OS_ERROR   error when activating a timer
 */
uint32_t os_timer_activate(os_timer_t tmr);

/**
 * Deactivate a timer
 * 
 * @param tmr timer handler
 * @retval OS_SUCCESS timer deactivated
 * @retval OS_ERROR   error when deactivating a timer
 */
uint32_t os_timer_deactivate(os_timer_t tmr);

/**
 * Change current timer values. Timer will be stopped and started again 
 * using the new values.
 * 
 * @param tmr timer handler
 * @param schedule_ms   first expiration time, in ms
 * @param reschedule_ms if you want a cyclic timer, use this parameter
 *                      to specify further expirations (ms)
 * @retval OS_SUCCESS timer deactivated
 * @retval OS_ERROR   error when deactivating a timer
 */
uint32_t os_timer_change(os_timer_t tmr, uint32_t schedule_ms, uint32_t reschedule_ms);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __OS_TIMER_H__ */


