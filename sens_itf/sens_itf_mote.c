#include <string.h>
#include <stdint.h>
#include "sens_itf.h"
#include "../os/os_defs.h"
#include "../os/os_timer.h"
#include "sens_util.h"
#include "../util/buf_io.h"
#include "../util/crc16.h"

// TODO: create fake function for SPI

#define SENS_ITF_DBG_FRAME 1

typedef struct sens_itf_acq_schedule_s
{
	uint8_t num_of_points;
	struct 
	{
		uint8_t index;
		uint32_t sampling_time_x250ms;
        uint32_t counter;
	} points[SENS_ITF_MAX_POINTS];
} sens_itf_acq_schedule_t;

static sens_itf_point_ctrl_t sensor_points;
static sens_itf_cmd_brd_id_t board_info;
static const uint8_t datatype_sizes[] = { 1, 1, 2, 2, 4, 4, 8, 8, 4, 8 }; // check sens_itf_datatypes_e order
static sens_itf_acq_schedule_t acquisition_schedule;
static os_timer_t acquistion_timer = 0;

static void sens_itf_mote_wake_up_sensor(void)
{

}

static void sens_itf_mote_sleep_sensor(void)
{

}

static uint8_t sens_itf_mote_identify_sensor(void)
{
}

static uint8_t sens_itf_mote_send_frame(uint8_t *frame, uint8_t size)
{
    sens_util_dump_frame(frame, size);
    return size;
}

static uint8_t sens_itf_mote_get_point_type(uint8_t point)
{
    return sensor_points.points[point].desc.type;
}


uint8_t sens_itf_mote_send_cmd(sens_itf_cmd_req_t *cmd, sens_itf_cmd_res_t * ans)
{
    static uint8_t frame[SENS_ITF_MAX_FRAME_SIZE];
    uint8_t frame_size, ret = 0;
    frame_size = sens_itf_pack_cmd_req(cmd,frame);

    if (frame_size > 0)
        ret =  sens_itf_mote_send_frame(frame, frame_size);

    return ret;

	// calc CRC !!!!

	// wake up sensor

	// packing / unpacking of cmds

	// sleep sensor
}

static uint8_t sens_itf_mote_check_version(uint8_t ver)
{
	uint8_t ret = 1;
	sens_itf_cmd_req_t cmd;
	sens_itf_cmd_res_t ans;

	memset(&cmd, 0, sizeof(cmd));
	memset(&ans, 0, sizeof(ans));

	cmd.hdr.size = 1;
	cmd.hdr.addr = SENS_ITF_REGMAP_ITF_VERSION;

	if (0 == sens_itf_mote_send_cmd(&cmd, &ans))
	{
		if ((SENS_ITF_ANS_OK == ans.hdr.status) && (ver == ans.payload.itf_version_cmd.version))
		{
			ret = 0;
		}
	}
	return ret;
}

static uint8_t sens_itf_mote_get_brd_id(sens_itf_cmd_res_t *ans)
{
	uint8_t ret = 1;
	sens_itf_cmd_req_t cmd;

	cmd.hdr.size = 1;
	cmd.hdr.addr = SENS_ITF_REGMAP_BRD_ID;

	if (0 == sens_itf_mote_send_cmd(&cmd, ans))
	{
		if (SENS_ITF_ANS_OK == ans->hdr.status)
		{
			ret = 0;
		}
	}
	return ret;
}

static uint8_t sens_itf_mote_get_points_desc(void)
{
	uint8_t res = 0;
	uint8_t n;
	sens_itf_cmd_req_t cmd;
	sens_itf_cmd_res_t ans;

	for (n = 0; n < sensor_points.num_of_points; n++)
	{
		cmd.hdr.addr = n + SENS_ITF_REGMAP_POINT_DESC_1;
		memset(&ans, 0, sizeof(ans));
		if (0 == sens_itf_mote_send_cmd(&cmd, &ans))
		{
			SENS_UTIL_ASSERT(ans.payload.point_desc_cmd.type <= SENS_ITF_DT_DOUBLE);
			memcpy(&(sensor_points.points[n].desc), &(ans.payload.point_desc_cmd), sizeof(sens_itf_cmd_point_desc_t));
		}
		else
		{
			res = 1;
			break;
		}
	}
	return res;
}

static void sens_itf_mote_build_acquisition_schedule(void)
{
	uint8_t n, m;

    acquisition_schedule.num_of_points = 0;

    for (n = 0, m = 0; n < board_info.num_of_points; n++)
    {
        if ((sensor_points.points[n].desc.access_rights & SENS_ITF_ACCESS_READ_ONLY) &&
            (sensor_points.points[n].desc.sampling_time_x250ms > 0))
        {
            acquisition_schedule.points[m].index = n; 
            acquisition_schedule.points[m].counter = sensor_points.points[n].desc.sampling_time_x250ms;;
            acquisition_schedule.points[m].sampling_time_x250ms = sensor_points.points[n].desc.sampling_time_x250ms;

            m++;
            acquisition_schedule.num_of_points++;
        }
    }
}

static void sens_itf_mote_read_point(uint8_t index)
{
    sens_util_log(1,"Reading point %d\n", index);
}

static void sens_itf_mote_acquisition_timer_func(void)
{
    uint8_t n;

    for (n = 0; n < acquisition_schedule.num_of_points; n++)
    {
        acquisition_schedule.points[n].counter--;
        if (acquisition_schedule.points[n].counter == 0)
        {
            sens_itf_mote_read_point(acquisition_schedule.points[n].index);
            acquisition_schedule.points[n].counter = acquisition_schedule.points[n].sampling_time_x250ms;
        }
    }
    
}
static void sens_itf_mote_create_acquisition_timer(void)
{
    acquistion_timer = os_timer_create((os_timer_func)sens_itf_mote_acquisition_timer_func, 0, 250, 250, 1);
}

uint8_t sens_itf_mote_init(void)
{
	sens_itf_cmd_res_t ans;

	memset(&ans, 0, sizeof(ans));

	memset(&sensor_points, 0, sizeof(sensor_points));
	memset(&board_info, 0, sizeof(board_info));
	memset(&acquisition_schedule, 0, sizeof(acquisition_schedule));

    #ifndef __CMD_DEBUG__

	if (0 == sens_itf_mote_check_version(SENS_ITF_LATEST_VERSION))
	{
		if (0 == sens_itf_mote_get_brd_id(&ans))
		{
			memcpy(&board_info, &(ans.payload.brd_id_cmd), sizeof(board_info));
			
			if (board_info.num_of_points <= SENS_ITF_MAX_POINTS)
			{
				sensor_points.num_of_points = board_info.num_of_points;
				if (0 == sens_itf_mote_get_points_desc())
				{
					sens_itf_mote_build_acquisition_schedule();
                    if (acquisition_schedule.num_of_points > 0)
                        sens_itf_mote_create_acquisition_timer();
				}
			}
		}
	}

    #endif

	return 1;
}



