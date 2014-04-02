#include <stdint.h>
#include "util\sens_util.h"
#include "./sens_itf/sens_itf.h"
#include "./owsn/board.h"
#include "./owsn/scheduler.h"

//void setUp(void) {}
//void tearDown(void) {}

#ifndef __CMD_DEBUG__

int main(void)
{
    sens_util_log_start();
	sens_itf_mote_init();
    board_init();
    scheduler_init();
    scheduler_start();
    sens_util_log_stop();
    
    return 0;
}

#endif
