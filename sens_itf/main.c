#include <stdint.h>
#include "../os/os_util.h"
#include "osens.h"
#include "osens_itf.h"
#include "../owsn/board.h"
#include "../owsn/scheduler.h"

#ifndef __CMD_DEBUG__

void setUp(void) {}
void tearDown(void) {}

extern void osens_sensor_main(void);
extern uint8_t osens_mote_init_v2(void);

int main(void)
{
    os_util_log_start();
    osens_mote_init_v2();
    os_util_log_stop();
    
    return 0;
}

#endif
