#include <stdint.h>
#include "../os/os_util.h"
#include "sens_itf.h"
#include "../owsn/board.h"
#include "../owsn/scheduler.h"

void setUp(void) {}
void tearDown(void) {}

#ifndef __CMD_DEBUG__

extern void sens_itf_sensor_main(void);

int main(void)
{
    os_util_log_start();
    sens_itf_mote_main();
    os_util_log_stop();
    
    return 0;
}

#endif
