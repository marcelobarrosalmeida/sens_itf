#include <string.h>
#include <stdint.h>
#include "sens_itf.h"
#include "../os/os_defs.h"
#include "../os/os_timer.h"
#include "../os/os_kernel.h"
#include "../os/os_serial.h"
#include "../os/os_util.h"
#include "../util/buf_io.h"
#include "../util/crc16.h"

// TODO: create fake function for SPI

#define SENS_ITF_DBG_FRAME 1
#define SENS_ITF_OUTPUT    1

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
static os_serial_t serial = 0;
static uint8_t frame[SENS_ITF_MAX_FRAME_SIZE];
static sens_itf_cmd_req_t cmd;
static sens_itf_cmd_res_t ans;


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
    int16_t sent;
    //os_util_dump_frame(frame, size);
    sent = os_serial_write(serial, frame, size);
    return (sent < 0 ? 0 : (uint8_t) sent); // CHECK AGAIN
}

static uint8_t sens_itf_mote_recv_frame(uint8_t *frame, uint8_t size)
{
    int16_t recv;
    
    recv = os_serial_read(serial, frame, size);
    if (recv > 0)
    {
        //os_util_dump_frame(frame, recv);
    }
    
    return (recv < 0 ? 0 : (uint8_t) recv); // CHECK AGAIN
}

static uint8_t sens_itf_mote_get_point_type(uint8_t point)
{
    return sensor_points.points[point].desc.type;
}


uint8_t sens_itf_mote_send_cmd(sens_itf_cmd_req_t *cmd, sens_itf_cmd_res_t * ans)
{
    
    uint8_t frame_size, ret = 0;
    frame_size = sens_itf_pack_cmd_req(cmd,frame);

    if (frame_size > 0)
    {
        ret =  sens_itf_mote_send_frame(frame, frame_size);
        if (ret == frame_size)
        {
            ret = sens_itf_mote_recv_frame(frame, 6);

        }
    }
        

    return ret;

}

static uint8_t sens_itf_mote_check_version(uint8_t ver)
{
    uint8_t size;
    uint8_t cmd_size = 4;
    uint8_t ans_size = 6;

    cmd.hdr.addr = SENS_ITF_REGMAP_ITF_VERSION;

    size = sens_itf_pack_cmd_req(&cmd, frame);
    if (size != cmd_size)
        return 0;

    if (sens_itf_mote_send_frame(frame, cmd_size) != cmd_size)
        return 0;

    os_kernel_sleep(1500);

    size = sens_itf_mote_recv_frame(frame, ans_size);
    if (size != ans_size)
        return 0;

    size = sens_itf_unpack_cmd_res(&ans, frame, ans_size);
    if (size != ans_size)
        return 0;

    if ((SENS_ITF_ANS_OK == ans.hdr.status) && (ver == ans.payload.itf_version_cmd.version))
        return 1;
    else
        return 0;
}

static uint8_t sens_itf_mote_get_brd_id(void)
{
	uint8_t size;
    uint8_t cmd_size = 4;
    uint8_t ans_size = 28;

	cmd.hdr.size = cmd_size;
	cmd.hdr.addr = SENS_ITF_REGMAP_BRD_ID;

    size = sens_itf_pack_cmd_req(&cmd, frame);
    if (size != cmd_size)
        return 0;

    if (sens_itf_mote_send_frame(frame, cmd_size) != cmd_size)
        return 0;

    os_kernel_sleep(1500);

    size = sens_itf_mote_recv_frame(frame, ans_size);
    if (size != ans_size)
        return 0;

    size = sens_itf_unpack_cmd_res(&ans, frame, ans_size);
    if (size != ans_size)
        return 0;

	return 1;
}

static uint8_t sens_itf_mote_get_points_desc(uint8_t point)
{
	uint8_t size;
    uint8_t cmd_size = 4;
    uint8_t ans_size = 20;

    cmd.hdr.addr = SENS_ITF_REGMAP_POINT_DESC_1 + point;

    size = sens_itf_pack_cmd_req(&cmd, frame);
    if (size != cmd_size)
        return 0;

    if (sens_itf_mote_send_frame(frame, cmd_size) != cmd_size)
        return 0;

    os_kernel_sleep(1500);

    size = sens_itf_mote_recv_frame(frame, ans_size);
    if (size != ans_size)
        return 0;

    size = sens_itf_unpack_cmd_res(&ans, frame, ans_size);
    if (size != ans_size)
        return 0;

    return 1;
}

static uint8_t sens_itf_mote_get_points_value(uint8_t point)
{
	uint8_t size;
    uint8_t cmd_size = 4;
    uint8_t ans_size = 6 + datatype_sizes[sensor_points.points[point].desc.type];

    cmd.hdr.addr = SENS_ITF_REGMAP_READ_POINT_DATA_1 + point;

    size = sens_itf_pack_cmd_req(&cmd, frame);
    if (size != cmd_size)
        return 0;

    if (sens_itf_mote_send_frame(frame, cmd_size) != cmd_size)
        return 0;

    os_kernel_sleep(1500);

    size = sens_itf_mote_recv_frame(frame, ans_size);
    if (size != ans_size)
        return 0;

    size = sens_itf_unpack_cmd_res(&ans, frame, ans_size);
    if (size != ans_size)
        return 0;

    return 1;
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
    uint8_t ret;
    OS_UTIL_LOG(1,("Reading point %d\n", index));
    ret = sens_itf_mote_get_points_value(index);
    if (index == 0 && ret == 1)
    {
        OS_UTIL_LOG(SENS_ITF_OUTPUT, ("Point 0 = %d\n",ans.payload.point_value_cmd.value.u8));
    }
    

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
    uint8_t n;
    uint8_t ret = 0;
    os_serial_options_t serial_options = { OS_SERIAL_BR_115200, OS_SERIAL_PR_NONE, OS_SERIAL_PB_1, 23};

	memset(&cmd, 0, sizeof(cmd));
	memset(&ans, 0, sizeof(ans));

    memset(&sensor_points, 0, sizeof(sensor_points));
	memset(&board_info, 0, sizeof(board_info));
	memset(&acquisition_schedule, 0, sizeof(acquisition_schedule));

    serial = os_serial_open(serial_options);

    if (sens_itf_mote_check_version(SENS_ITF_LATEST_VERSION))
    {
        OS_UTIL_LOG(SENS_ITF_OUTPUT,("Interface version %d\n\n",ans.payload.itf_version_cmd.version));

        if (sens_itf_mote_get_brd_id())
        {
            memcpy(&board_info, &ans.payload.brd_id_cmd, sizeof(sens_itf_cmd_brd_id_t));

            OS_UTIL_LOG(SENS_ITF_OUTPUT, ("Board info\n"));
            OS_UTIL_LOG(SENS_ITF_OUTPUT, ("==========\n"));
            OS_UTIL_LOG(SENS_ITF_OUTPUT, ("Manufactor : %-8s\n",board_info.manufactor));
            OS_UTIL_LOG(SENS_ITF_OUTPUT, ("Model      : %-8s\n",board_info.model));
            OS_UTIL_LOG(SENS_ITF_OUTPUT, ("ID         : %08X\n",board_info.sensor_id));
            OS_UTIL_LOG(SENS_ITF_OUTPUT, ("HW REV     : %02X\n",board_info.hardware_revision));
            OS_UTIL_LOG(SENS_ITF_OUTPUT, ("Capabilties: %02X\n", board_info.cabalities));
            OS_UTIL_LOG(SENS_ITF_OUTPUT, ("Points     : %d\n\n", board_info.num_of_points));

			if (board_info.num_of_points <= SENS_ITF_MAX_POINTS)
			{
                uint8_t all_points_ok = 1;
				sensor_points.num_of_points = board_info.num_of_points;
                for (n = 0; n < sensor_points.num_of_points; n++)
	            {
                    if (sens_itf_mote_get_points_desc(n))
                    {
                        //sens_itf_point_ctrl_t sensor_points
                        memcpy(&sensor_points.points[n].desc, &ans.payload.point_desc_cmd, sizeof(sens_itf_cmd_point_desc_t));
                        OS_UTIL_LOG(SENS_ITF_OUTPUT, ("Point %02d info\n", n));
                        OS_UTIL_LOG(SENS_ITF_OUTPUT, ("=============\n"));
                        OS_UTIL_LOG(SENS_ITF_OUTPUT, ("Name     : %-8s\n", sensor_points.points[n].desc.name));
                        OS_UTIL_LOG(SENS_ITF_OUTPUT, ("Type     : %d\n", sensor_points.points[n].desc.type));
                        OS_UTIL_LOG(SENS_ITF_OUTPUT, ("Unit     : %d\n", sensor_points.points[n].desc.unit));
                        OS_UTIL_LOG(SENS_ITF_OUTPUT, ("Rights   : %02X\n", sensor_points.points[n].desc.access_rights));
                        OS_UTIL_LOG(SENS_ITF_OUTPUT, ("Sampling : %d\n\n", sensor_points.points[n].desc.sampling_time_x250ms));
                    }
                    else
                        all_points_ok = 0;
                }
                if (all_points_ok)
                {
                    sens_itf_mote_build_acquisition_schedule();

                    if (acquisition_schedule.num_of_points > 0)
                    {
                        OS_UTIL_LOG(SENS_ITF_OUTPUT, ("Acquisiton scheduling\n", n));
                        OS_UTIL_LOG(SENS_ITF_OUTPUT, ("=====================\n"));

                        for (n = 0; n < acquisition_schedule.num_of_points; n++)
                        {
                            OS_UTIL_LOG(SENS_ITF_OUTPUT, ("[%d] point %02d at %dms\n", n,acquisition_schedule.points[n].index,acquisition_schedule.points[n].sampling_time_x250ms*250));
                        }

                        sens_itf_mote_create_acquisition_timer();
                    }

                    ret = 1;
                }
			}
        }
    }

	return ret;
}

void sens_itf_mote_main(void)
{
    sens_itf_mote_init();

    while (1){};
    //sens_itf_sensor_main();
	//sens_itf_mote_init();
    //board_init();
    //scheduler_init();
    //scheduler_start();

}


