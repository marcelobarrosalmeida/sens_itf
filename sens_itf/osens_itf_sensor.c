#include <string.h>
#include <stdint.h>
#include "osens.h"
#include "osens_itf.h"
#include "../os/os_defs.h"
#include "../os/os_timer.h"
#include "../os/os_kernel.h"
#include "../os/os_util.h"
#include "../util/buf_io.h"
#include "../util/crc16.h"
#include "../pt/pt.h"

// TODO: create fake function for SPI

#define SENS_ITF_SENSOR_DBG_FRAME     1
#define OSENS_DBG_FRAME 1
#define SENS_ITF_SENSOR_NUM_OF_POINTS 5

static uint8_t main_svr_addr[OSENS_SERVER_ADDR_SIZE];
static uint8_t secon_svr_addr[OSENS_SERVER_ADDR_SIZE];
static uint8_t rx_frame[OSENS_MAX_FRAME_SIZE];
static volatile uint8_t num_rx_bytes;
static os_timer_t rx_trmout_timer ;
static os_timer_t acq_data_timer;
static osens_point_ctrl_t sensor_points;
static osens_brd_id_t board_info;
static struct pt pt_acq;
static struct pt pt_data;
static volatile uint8_t frame_timeout;
static volatile uint8_t acq_data ;

static uint8_t osens_get_point_type(uint8_t point)
{
    return sensor_points.points[point].desc.type;
}

static uint8_t osens_get_number_of_points(void)
{
    return SENS_ITF_SENSOR_NUM_OF_POINTS;
}

static osens_point_desc_t *osens_get_point_desc(uint8_t point)
{
    osens_point_desc_t *d = 0;

    if (point < osens_get_number_of_points())
        d = &sensor_points.points[point].desc;

    return d;
}

static osens_point_t *osens_get_point_value(uint8_t point)
{
    osens_point_t *v = 0;

    if (point < osens_get_number_of_points())
        v = &sensor_points.points[point].value;

    return v;
}

static uint8_t osens_set_point_value(uint8_t point, osens_point_t *v)
{
    uint8_t ret = 0;

    if (point < osens_get_number_of_points())
    {
        sensor_points.points[point].value = *v;
        ret = 1;
    }
    else
    {
        ret = 0;
    }

    return ret;
}

static osens_brd_id_t *osens_get_board_info(void)
{
    return &board_info;
}

static uint8_t osens_sensor_send_frame(uint8_t *frame, uint8_t size)
{
    os_util_dump_frame(frame, size);
    return size;
}

static uint8_t osens_sensor_check_register_map(osens_cmd_req_t *cmd, osens_cmd_res_t *ans, uint8_t *frame)
{
    uint8_t size = 0;
    if ( // check global register map for valid address ranges
                ((cmd->hdr.addr > OSENS_REGMAP_SVR_SEC_ADDR) && 
                (cmd->hdr.addr < OSENS_REGMAP_POINT_DESC_1)) ||
                (cmd->hdr.addr > OSENS_REGMAP_WRITE_POINT_DATA_32) ||
                // check local register map - reading
                ((cmd->hdr.addr >= OSENS_REGMAP_READ_POINT_DATA_1) && 
                (cmd->hdr.addr <= OSENS_REGMAP_READ_POINT_DATA_32) &&
                ((cmd->hdr.addr - OSENS_REGMAP_READ_POINT_DATA_1) >= osens_get_number_of_points())) ||
                // check local register map - writing
                ((cmd->hdr.addr >= OSENS_REGMAP_WRITE_POINT_DATA_1) && 
                (cmd->hdr.addr <= OSENS_REGMAP_WRITE_POINT_DATA_32) &&
                ((cmd->hdr.addr - OSENS_REGMAP_WRITE_POINT_DATA_1) >= osens_get_number_of_points())))
    {
        OS_UTIL_LOG(SENS_ITF_SENSOR_DBG_FRAME, ("Invalid register address %02X",cmd->hdr.addr));
        ans->hdr.status = OSENS_ANS_REGISTER_NOT_IMPLEMENTED;
        size = osens_pack_cmd_res(ans, frame);
    }
    return size;
}

static uint8_t osens_sensor_writings(osens_cmd_req_t *cmd, osens_cmd_res_t *ans, uint8_t *frame)
{
    uint8_t size = 0;
    if ((cmd->hdr.addr >= OSENS_REGMAP_WRITE_POINT_DATA_1) && 
        (cmd->hdr.addr <= OSENS_REGMAP_WRITE_POINT_DATA_32))
    {
        uint8_t point = cmd->hdr.addr - OSENS_REGMAP_WRITE_POINT_DATA_1;
        uint8_t acr = osens_get_point_desc(point)->access_rights & OSENS_ACCESS_WRITE_ONLY;
            
        if (acr)
        {
            ans->hdr.status = OSENS_ANS_OK;
            osens_set_point_value(point,&cmd->payload.point_value_cmd);
        }
        else
        {
            OS_UTIL_LOG(SENS_ITF_SENSOR_DBG_FRAME, ("Point %d does not allow writings",point));  
            ans->hdr.status = OSENS_ANS_READY_ONLY;
        }
        size = osens_pack_cmd_res(ans, frame);
    }
    return size;
}

static uint8_t osens_sensor_readings(osens_cmd_req_t *cmd, osens_cmd_res_t *ans, uint8_t *frame)
{
    uint8_t size = 0;
    if ((cmd->hdr.addr >= OSENS_REGMAP_READ_POINT_DATA_1) &&
        (cmd->hdr.addr <= OSENS_REGMAP_READ_POINT_DATA_32))
    {
        uint8_t point = cmd->hdr.addr - OSENS_REGMAP_READ_POINT_DATA_1;
        uint8_t acr = osens_get_point_desc(point)->access_rights & OSENS_ANS_READY_ONLY;
            
        if (acr)
        {
            ans->hdr.status = OSENS_ANS_OK;
            ans->payload.point_value_cmd = *osens_get_point_value(point);
        }
        else
        {
            OS_UTIL_LOG(SENS_ITF_SENSOR_DBG_FRAME, ("Point %d does not allow readings",point));
            ans->hdr.status = OSENS_ANS_WRITE_ONLY;
        }
        size = osens_pack_cmd_res(ans, frame);
    }
    return size;
}

static uint8_t osens_check_other_cmds(osens_cmd_req_t *cmd, osens_cmd_res_t *ans, uint8_t *frame)
{
    uint8_t size = 0;
    switch (cmd->hdr.addr)
    {
        case OSENS_REGMAP_ITF_VERSION:
            ans->payload.itf_version_cmd.version = OSENS_LATEST_VERSION;
            break;
        case OSENS_REGMAP_BRD_ID:
            memcpy(&ans->payload.brd_id_cmd,osens_get_board_info(),sizeof(osens_brd_id_t));
            break;
        case OSENS_REGMAP_BRD_STATUS:
            ans->payload.brd_status_cmd.status = 0; // TBD
            break;
        case OSENS_REGMAP_BRD_CMD:
            ans->payload.command_res_cmd.status = 0; // TBD
            break;
        case OSENS_REGMAP_READ_BAT_STATUS:
            ans->payload.bat_status_cmd.status = 0; // TBD
            break;
        case OSENS_REGMAP_READ_BAT_CHARGE:
            ans->payload.bat_charge_cmd.charge = 100; // TBD
            break;
        case OSENS_REGMAP_SVR_MAIN_ADDR:
            memcpy(ans->payload.svr_addr_cmd.addr,main_svr_addr, OSENS_SERVER_ADDR_SIZE); 
            break;
        case OSENS_REGMAP_SVR_SEC_ADDR:
            memcpy(ans->payload.svr_addr_cmd.addr,secon_svr_addr, OSENS_SERVER_ADDR_SIZE);
            break;
        default:
            break;
    }

    if ((cmd->hdr.addr >= OSENS_REGMAP_POINT_DESC_1) && (cmd->hdr.addr <= OSENS_REGMAP_POINT_DESC_32))
    {
        uint8_t point = cmd->hdr.addr - OSENS_REGMAP_POINT_DESC_1;
        memcpy(&ans->payload.point_desc_cmd, &sensor_points.points[point].desc, sizeof(osens_point_desc_t));
    }

    ans->hdr.status = OSENS_ANS_OK;
    size = osens_pack_cmd_res(ans, frame);
    return size;
}
static void osens_process_cmd(uint8_t *frame, uint8_t num_rx_bytes)
{
    uint8_t ret;
    uint8_t size = 0;
    osens_cmd_req_t cmd;
    osens_cmd_res_t ans;

    ret = osens_unpack_cmd_req(&cmd, frame, num_rx_bytes);

    if (ret > 0)
    {
        ans.hdr.addr = cmd.hdr.addr;
        size = osens_sensor_check_register_map(&cmd, &ans,frame);
        if (size == 0)
            size = osens_sensor_writings(&cmd, &ans,frame);
        if (size == 0)
            size = osens_sensor_readings(&cmd, &ans,frame);
        if (size == 0)
            size = osens_check_other_cmds(&cmd, &ans,frame);
        if (size == 0)
        {
            ans.hdr.status = OSENS_ANS_ERROR;
        }
        size = osens_pack_cmd_res(&ans,frame);
        osens_sensor_send_frame(frame, size);
    }
}

void osens_init_point_db(void)
{
    uint8_t n;
    uint8_t *point_names[OSENS_POINT_NAME_SIZE] = { "TEMP", "HUMID", "FIRE", "ALARM", "OPENCNT" };
    uint8_t data_types[OSENS_POINT_NAME_SIZE] = {OSENS_DT_FLOAT, OSENS_DT_FLOAT, OSENS_DT_U8, 
        OSENS_DT_U8, OSENS_DT_U32};
    uint8_t access_rights[OSENS_POINT_NAME_SIZE] = { OSENS_ACCESS_READ_ONLY, OSENS_ACCESS_READ_ONLY, 
        OSENS_ACCESS_READ_ONLY, OSENS_ACCESS_WRITE_ONLY, OSENS_ACCESS_READ_WRITE};
    uint32_t sampling_time[OSENS_POINT_NAME_SIZE] = {4*10, 4*30, 4*1, 0, 0};

	memset(&sensor_points, 0, sizeof(sensor_points));
	memset(&board_info, 0, sizeof(board_info));
	
    strcpy(board_info.model, "KL46Z");
    strcpy(board_info.manufactor, "TESLA");
    board_info.sensor_id = 0xDEADBEEF;
    board_info.hardware_revision = 0x01;
    board_info.num_of_points = SENS_ITF_SENSOR_NUM_OF_POINTS;
    board_info.cabalities = OSENS_CAPABILITIES_DISPLAY |
        OSENS_CAPABILITIES_WPAN_STATUS | 
        OSENS_CAPABILITIES_BATTERY_STATUS;

    sensor_points.num_of_points = SENS_ITF_SENSOR_NUM_OF_POINTS;

    for (n = 0; n < SENS_ITF_SENSOR_NUM_OF_POINTS; n++)
    {
        strcpy(sensor_points.points[n].desc.name, point_names[n]);
        sensor_points.points[n].desc.type = data_types[n];
        sensor_points.points[n].desc.unit = 0; // TDB
        sensor_points.points[n].desc.access_rights = access_rights[n];
        sensor_points.points[n].desc.sampling_time_x250ms = sampling_time[n];
        sensor_points.points[n].value.type = data_types[n];
    }
}

static void osens_rx_tmrout_timer_func(void)
{
    //static int debug = 0;
    //if (++debug == 200)
    //{
    //    uint16_t crc;
    //    uint8_t x[] = {2, OSENS_REGMAP_ITF_VERSION, 0x00, 0x00};
    //    crc = crc16_calc(x, 2);
    //    buf_io_put16_tl(crc, &x[2]);
    //    debug = 0;
    //    memcpy(rx_frame, x, 4);
    //    num_rx_bytes = 4;
    //}
    frame_timeout = 1;
}

static void osens_acq_data_timer_func(void)
{
    acq_data = 1;
}

uint8_t osens_sensor_init(void)
{

    osens_init_point_db();
    memcpy(main_svr_addr,"1212121212121212",OSENS_SERVER_ADDR_SIZE);
    memcpy(secon_svr_addr,"aabbccddeeff1122",OSENS_SERVER_ADDR_SIZE);
    num_rx_bytes = 0;
    acq_data = 0;
    frame_timeout = 0;
    rx_trmout_timer = os_timer_create((os_timer_func) osens_rx_tmrout_timer_func, 0, 50, 0, 1);
    acq_data_timer = os_timer_create((os_timer_func) osens_acq_data_timer_func, 0, 1000, 0, 1);

    return 1;
}

// Serial or SPI interrupt, called when a new byte is received
static void osens_sensor_rx_byte(uint8_t value)
{
    // DISABLE INTERRUPTS
    if (frame_timeout)
        return;

    if (num_rx_bytes < OSENS_MAX_FRAME_SIZE)
        rx_frame[num_rx_bytes] = value;
    
    num_rx_bytes++;
    if (num_rx_bytes >= OSENS_MAX_FRAME_SIZE)
        num_rx_bytes = 0;

    os_timer_change(rx_trmout_timer, 50, 0);
    // ENABLE INTERRUPTS
}

static int pt_data_func(struct pt *pt)
{
    PT_BEGIN(pt);

    while (1)
    {
        // wait a frame timeout
        PT_WAIT_UNTIL(pt, frame_timeout == 1);

        if (num_rx_bytes > 0)
        {
            // process it
            osens_process_cmd(rx_frame, num_rx_bytes);
            num_rx_bytes = 0;
        }

        // restart reception
        frame_timeout = 0;
        os_timer_change(rx_trmout_timer, 50, 0);
    }

    PT_END(pt);
}

static int pt_acq_func(struct pt *pt)
{
    PT_BEGIN(pt);

    while (1)
    {
        // wait job
        PT_WAIT_UNTIL(pt, acq_data == 1);
        // read data from sensor and update points
        // {}
        acq_data = 0;
    }

    PT_END(pt);
}

void osens_sensor_main(void)
{

    osens_sensor_init();

    PT_INIT(&pt_data);
    PT_INIT(&pt_acq);

    frame_timeout = 0;
    acq_data = 0;

    while(1)
    {
        pt_data_func(&pt_data);
        pt_acq_func(&pt_acq);
    }    

    //while (1)
    //{
    //    if (process_frame)
    //    {
    //        osens_process_cmd();
    //        process_frame = 0;
    //        num_rx_bytes = 0;
    //        // enable rx interrupt
    //    }
    //    else
    //    {
    //        os_kernel_sleep(50);
    //    }
    //}

}

