#include <string.h>
#include <stdint.h>
#include "sens_itf.h"
#include "../os/os_defs.h"
#include "../os/os_timer.h"
#include "sens_util.h"
#include "../util/buf_io.h"
#include "../util/crc16.h"


// TODO: create fake function for SPI

#define SENS_ITF_SENSOR_DBG_FRAME     1
#define SENS_ITF_DBG_FRAME 1
#define SENS_ITF_SENSOR_NUM_OF_POINTS 5

static uint8_t frame[SENS_ITF_MAX_FRAME_SIZE];
static volatile uint8_t num_rx_bytes;
static os_timer_t rx_trmout_timer = 0;
static volatile uint8_t process_frame;
static sens_itf_point_ctrl_t sensor_points;
static sens_itf_cmd_brd_id_t board_info;

uint8_t sens_itf_get_point_type(uint8_t point)
{
    return sensor_points.points[point].desc.type;
}

uint8_t sens_itf_get_number_of_points(void)
{
    return SENS_ITF_SENSOR_NUM_OF_POINTS;
}

sens_itf_cmd_point_desc_t *sens_itf_get_point_desc(uint8_t point)
{
    sens_itf_cmd_point_desc_t *d = 0;

    if (point < sens_itf_get_number_of_points())
        d = &sensor_points.points[point].desc;

    return d;
}

sens_itf_cmd_point_t *sens_itf_get_point_value(uint8_t point)
{
    sens_itf_cmd_point_t *v = 0;

    if (point < sens_itf_get_number_of_points())
        v = &sensor_points.points[point].value;

    return v;
}

uint8_t sens_itf_set_point_value(uint8_t point, sens_itf_cmd_point_t *v)
{
    uint8_t ret = 0;

    if (point < sens_itf_get_number_of_points())
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

sens_itf_cmd_brd_id_t *sens_itf_get_board_info(void)
{
    return &board_info;
}

uint8_t sens_itf_sensor_unpack_command_req(sens_itf_cmd_req_t *cmd, uint8_t *frame, uint8_t frame_size)
{
    uint8_t *buf = frame;
    uint16_t crc;
    uint16_t frame_crc;
    uint8_t size;

    if (frame_size < 3)
    {
        sens_util_log(SENS_ITF_DBG_FRAME, "Invalid frame size %d", frame_size);
        return 0;
    }
    
    // minimal header decoding
    cmd->hdr.size = buf_io_get8_fl_ap(buf);
    cmd->hdr.addr = buf_io_get8_fl_ap(buf);

    frame_crc = buf_io_get16_fl(&frame[cmd->hdr.size]);
    crc = crc16_calc(frame, cmd->hdr.size);
    cmd->crc = frame_crc;

    if (frame_crc != crc)
    {
        sens_util_log(SENS_ITF_DBG_FRAME, "Invalid CRC %04X <> %04X", frame_crc, crc);
        return 0;
    }
    
    switch (cmd->hdr.addr)
    {
    case SENS_ITF_REGMAP_BRD_CMD:
        cmd->payload.command_cmd.cmd = buf_io_get8_fl_ap(buf);
        break;
    case SENS_ITF_REGMAP_WRITE_BAT_STATUS:
        cmd->payload.bat_status_cmd.status = buf_io_get8_fl_ap(buf);
        break;
    case SENS_ITF_REGMAP_WRITE_BAT_CHARGE:
        cmd->payload.bat_charge_cmd.charge = buf_io_get8_fl_ap(buf);
        break;
    case SENS_ITF_REGMAP_DSP_WRITE:
        cmd->payload.write_display_cmd.line = buf_io_get8_fl_ap(buf);
        memcpy(cmd->payload.write_display_cmd.msg,buf,SENS_ITF_DSP_MSG_MAX_SIZE);
        buf += SENS_ITF_DSP_MSG_MAX_SIZE;
        break;
    case SENS_ITF_REGMAP_WPAN_STATUS:
        cmd->payload.wpan_status_cmd.status = buf_io_get8_fl_ap(buf);
        break;
    case SENS_ITF_REGMAP_WPAN_STRENGTH:
        cmd->payload.wpan_strength_cmd.strenght = buf_io_get8_fl_ap(buf);
        break;
    default:
        break;
    }

    if ((cmd->hdr.addr >= SENS_ITF_REGMAP_WRITE_POINT_DATA_1) && 
        (cmd->hdr.addr <= SENS_ITF_REGMAP_WRITE_POINT_DATA_32))
    {
        uint8_t point = cmd->hdr.addr - SENS_ITF_REGMAP_WRITE_POINT_DATA_1;
        if (point < SENS_ITF_SENSOR_NUM_OF_POINTS)
        {
            // update db
            buf += sens_itf_unpack_point_value(sens_itf_get_point_value(point), buf);
            // update cmd
            cmd->payload.point_value_cmd = *sens_itf_get_point_value(point);
        }
    }

    size = cmd->hdr.size + 2; // + crc 
    return size;
}

uint8_t sens_itf_sensor_pack_command_res(sens_itf_cmd_res_t *cmd, uint8_t *frame)
{
    uint8_t *buf = &frame[1];
    uint8_t size = 0;
    uint16_t crc;

    buf_io_put8_tl_ap(cmd->hdr.addr, buf);
    buf_io_put8_tl_ap(cmd->hdr.status, buf);

    // only fill command when status is OK, otherwise an error will be reported
    if (cmd->hdr.status == SENS_ITF_ANS_OK)
    {
        switch (cmd->hdr.addr)
        {
        case SENS_ITF_REGMAP_ITF_VERSION:
            buf_io_put8_tl_ap(cmd->payload.itf_version_cmd.version, buf);
            break;
        case SENS_ITF_REGMAP_BRD_ID:
            memcpy(buf, cmd->payload.brd_id_cmd.model, SENS_ITF_MODEL_NAME_SIZE);
            buf += SENS_ITF_MODEL_NAME_SIZE;
            memcpy(buf, cmd->payload.brd_id_cmd.manufactor, SENS_ITF_MANUF_NAME_SIZE);
            buf += SENS_ITF_MODEL_NAME_SIZE;
            buf_io_put32_tl_ap(cmd->payload.brd_id_cmd.sensor_id, buf);
            buf_io_put8_tl_ap(cmd->payload.brd_id_cmd.hardware_revision, buf);
            buf_io_put8_tl_ap(cmd->payload.brd_id_cmd.num_of_points, buf);
            buf_io_put8_tl_ap(cmd->payload.brd_id_cmd.cabalities, buf);
            break;
        case SENS_ITF_REGMAP_BRD_STATUS:
            buf_io_put8_tl_ap(cmd->payload.brd_status_cmd.status, buf);
            break;
        case SENS_ITF_REGMAP_BRD_CMD:
            buf_io_put8_tl_ap(cmd->payload.command_res_cmd.status, buf);
            break;
        case SENS_ITF_REGMAP_READ_BAT_STATUS:
            buf_io_put8_tl_ap(cmd->payload.bat_status_cmd.status, buf);
            break;
        case SENS_ITF_REGMAP_READ_BAT_CHARGE:
            buf_io_put8_tl_ap(cmd->payload.bat_charge_cmd.charge, buf);
            break;
        case SENS_ITF_REGMAP_SVR_MAIN_ADDR:
        case SENS_ITF_REGMAP_SVR_SEC_ADDR:
            memcpy(buf, cmd->payload.svr_addr_cmd.addr, SENS_ITF_SERVER_ADDR_SIZE);
            buf += SENS_ITF_SERVER_ADDR_SIZE;
            break;
        }

        if ((cmd->hdr.addr >= SENS_ITF_REGMAP_POINT_DESC_1) &&
            (cmd->hdr.addr <= SENS_ITF_REGMAP_POINT_DESC_32))
        {
            uint8_t point = cmd->hdr.addr - SENS_ITF_REGMAP_POINT_DESC_1;
            if (point < SENS_ITF_SENSOR_NUM_OF_POINTS)
            {
                memcpy(buf, cmd->payload.point_desc_cmd.name, SENS_ITF_POINT_NAME_SIZE);
                buf += SENS_ITF_POINT_NAME_SIZE;
                buf_io_put8_tl_ap(cmd->payload.point_desc_cmd.type, buf);
                buf_io_put8_tl_ap(cmd->payload.point_desc_cmd.unit, buf);
                buf_io_put8_tl_ap(cmd->payload.point_desc_cmd.access_rights, buf);
                buf_io_put32_tl_ap(cmd->payload.point_desc_cmd.sampling_time_x250ms, buf);
            }
        }

        if ((cmd->hdr.addr >= SENS_ITF_REGMAP_READ_POINT_DATA_1) &&
            (cmd->hdr.addr <= SENS_ITF_REGMAP_READ_POINT_DATA_32))
        {
            uint8_t point = cmd->hdr.addr - SENS_ITF_REGMAP_READ_POINT_DATA_1;
            if (point < SENS_ITF_SENSOR_NUM_OF_POINTS)
            {
                buf += sens_itf_pack_point_value(sens_itf_get_point_value(point), buf);
                // update cmd
                cmd->payload.point_value_cmd = *sens_itf_get_point_value(point);
            }
        }
    }

    size = buf - frame;
    buf_io_put8_tl(size, frame);
    crc = crc16_calc(frame, size);
    cmd->crc = crc;
    cmd->hdr.size = size;
    buf_io_put16_tl(crc, buf);
    
    size += 2; // +crc 
    return size;
}

static uint8_t sens_itf_sensor_send_frame(uint8_t *frame, uint8_t size)
{
    sens_util_dump_frame(frame, size);
    return size;
}

static uint8_t sens_itf_sensor_check_register_map(sens_itf_cmd_req_t *cmd, sens_itf_cmd_res_t *ans, uint8_t *frame)
{
    uint8_t size = 0;
    if ( // check global register map for valid address ranges
                ((cmd->hdr.addr > SENS_ITF_REGMAP_SVR_SEC_ADDR) && 
                (cmd->hdr.addr < SENS_ITF_REGMAP_POINT_DESC_1)) ||
                (cmd->hdr.addr > SENS_ITF_REGMAP_WRITE_POINT_DATA_32) ||
                // check local register map - reading
                ((cmd->hdr.addr >= SENS_ITF_REGMAP_READ_POINT_DATA_1) && 
                (cmd->hdr.addr <= SENS_ITF_REGMAP_READ_POINT_DATA_32) &&
                ((cmd->hdr.addr - SENS_ITF_REGMAP_READ_POINT_DATA_1) >= sens_itf_get_number_of_points())) ||
                // check local register map - writing
                ((cmd->hdr.addr >= SENS_ITF_REGMAP_WRITE_POINT_DATA_1) && 
                (cmd->hdr.addr <= SENS_ITF_REGMAP_WRITE_POINT_DATA_32) &&
                ((cmd->hdr.addr - SENS_ITF_REGMAP_WRITE_POINT_DATA_1) >= sens_itf_get_number_of_points())))
    {
        sens_util_log(SENS_ITF_SENSOR_DBG_FRAME, "Invalid register address %02X",cmd->hdr.addr);
        ans->hdr.status = SENS_ITF_ANS_REGISTER_NOT_IMPLEMENTED;
        size = sens_itf_sensor_pack_command_res(ans, frame);
    }
    return size;
}

static uint8_t sens_itf_sensor_writings(sens_itf_cmd_req_t *cmd, sens_itf_cmd_res_t *ans, uint8_t *frame)
{
    uint8_t size = 0;
    if ((cmd->hdr.addr >= SENS_ITF_REGMAP_WRITE_POINT_DATA_1) && 
        (cmd->hdr.addr <= SENS_ITF_REGMAP_WRITE_POINT_DATA_32))
    {
        uint8_t point = cmd->hdr.addr - SENS_ITF_REGMAP_WRITE_POINT_DATA_1;
        uint8_t acr = sens_itf_get_point_desc(point)->access_rights & SENS_ITF_ACCESS_WRITE_ONLY;
            
        if (!acr)
        {
            sens_util_log(SENS_ITF_SENSOR_DBG_FRAME, "Point %d does not allow writings",point);                
            ans->hdr.status = SENS_ITF_ANS_READY_ONLY;
        }
        else
        {
            ans->hdr.status = SENS_ITF_ANS_OK;
            sens_itf_set_point_value(point,&cmd->payload.point_value_cmd);
        }
        size = sens_itf_sensor_pack_command_res(ans, frame);
    }
    return size;
}

static uint8_t sens_itf_sensor_readings(sens_itf_cmd_req_t *cmd, sens_itf_cmd_res_t *ans, uint8_t *frame)
{
    uint8_t size = 0;
    if ((cmd->hdr.addr >= SENS_ITF_REGMAP_READ_POINT_DATA_1) &&
        (cmd->hdr.addr <= SENS_ITF_REGMAP_READ_POINT_DATA_32))
    {
        uint8_t point = cmd->hdr.addr - SENS_ITF_REGMAP_READ_POINT_DATA_1;
        uint8_t acr = sens_itf_get_point_desc(point)->access_rights & SENS_ITF_ANS_READY_ONLY;
            
        if (!acr)
        {
            sens_util_log(SENS_ITF_SENSOR_DBG_FRAME, "Point %d does not allow readings",point);                
            ans->hdr.status = SENS_ITF_ANS_WRITE_ONLY;
        }
        else
        {
            ans->hdr.status = SENS_ITF_ANS_OK;
            sens_itf_cmd_point_t *pv = sens_itf_get_point_value(point);
            ans->payload.point_value_cmd = *pv;
        }
        size = sens_itf_sensor_pack_command_res(ans, frame);
    }
    return size;
}

static uint8_t sens_itf_check_other_cmds(sens_itf_cmd_req_t *cmd, sens_itf_cmd_res_t *ans, uint8_t *frame)
{
    uint8_t size = 0;
    switch (cmd->hdr.addr)
    {
        case SENS_ITF_REGMAP_ITF_VERSION:
            ans->payload.itf_version_cmd.version = SENS_ITF_LATEST_VERSION;
            break;
        case SENS_ITF_REGMAP_BRD_ID:
            memcpy(&ans->payload.brd_id_cmd,sens_itf_get_board_info(),sizeof(sens_itf_cmd_brd_id_t));
            break;
        case SENS_ITF_REGMAP_BRD_STATUS:
            ans->payload.brd_status_cmd.status = 0; // TBD
            break;
        case SENS_ITF_REGMAP_BRD_CMD:
            ans->payload.command_res_cmd.status = 0; // TBD
            break;
        case SENS_ITF_REGMAP_READ_BAT_STATUS:
            ans->payload.bat_status_cmd.status = 0; // TBD
            break;
        case SENS_ITF_REGMAP_READ_BAT_CHARGE:
            ans->payload.bat_charge_cmd.charge = 100; // TBD
            break;
        case SENS_ITF_REGMAP_SVR_MAIN_ADDR:
            memcpy(ans->payload.svr_addr_cmd.addr,"1111222233334444" , SENS_ITF_SERVER_ADDR_SIZE); // TBD
            break;
        case SENS_ITF_REGMAP_SVR_SEC_ADDR:
            memcpy(ans->payload.svr_addr_cmd.addr,"aaaabbbbccccdddd" , SENS_ITF_SERVER_ADDR_SIZE); // TBD
            break;

    }
    size = sens_itf_sensor_pack_command_res(ans, frame);
    return size;
}
static void sens_itf_process_cmd(void)
{
    uint8_t ret;
    uint8_t size = 0;
    sens_itf_cmd_req_t cmd;
    sens_itf_cmd_res_t ans;

    ret = sens_itf_sensor_unpack_command_req(&cmd, frame, num_rx_bytes);

    if (ret > 0)
    {
        ans.hdr.addr = cmd.hdr.addr;
        size = sens_itf_sensor_check_register_map(&cmd, &ans,frame);
        if (!size)
            size = sens_itf_sensor_writings(&cmd, &ans,frame);
        if (!size)
            size = sens_itf_sensor_readings(&cmd, &ans,frame);
        if (!size)
            size = size = sens_itf_check_other_cmds(&cmd, &ans,frame);
        if (!size)
        {
            ans.hdr.status = SENS_ITF_ANS_ERROR;
            size = sens_itf_sensor_pack_command_res(&ans,frame);
        }
        
        sens_itf_sensor_send_frame(frame, size);
    }
}

// Serial or SPI interrupt, called when a new byte is received
static void sens_itf_sensor_rx_byte(uint8_t value)
{
    // DISABLE INTERRUPTS
    if (process_frame)
        return;

    if (num_rx_bytes < SENS_ITF_MAX_FRAME_SIZE)
        frame[num_rx_bytes] = value;
    
    num_rx_bytes++;
    if (num_rx_bytes >= SENS_ITF_MAX_FRAME_SIZE)
        num_rx_bytes = 0;

    os_timer_change(rx_trmout_timer, 200, 0);
    // ENABLE INTERRUPTS
}

static void sens_itf_rx_tmrout_timer_func(void)
{
    // DISABLE_INTERRUPTS
    if (process_frame)
        return;

    if (num_rx_bytes >= 4)
    {
        process_frame = 1;
        // disable rx interrupt 
    }
    else
    {
        num_rx_bytes = 0;
    }
    // ENABLE INTERRUPTS
}

void sens_itf_sensor_main(void)
{

    while (1)
    {
        if (process_frame)
        {
            sens_itf_process_cmd();
            process_frame = 0;
            num_rx_bytes = 0;
            // enable rx interrupt
        }
        else
        {
            //sleep(50ms)
        }
    }

}

void sens_itf_init_point_db(void)
{
    uint8_t n;
    uint8_t *point_names[SENS_ITF_POINT_NAME_SIZE] = { "TEMP", "HUMID", "FIRE", "ALARM", "OPENCNT" };
    uint8_t data_types[SENS_ITF_POINT_NAME_SIZE] = {SENS_ITF_DT_FLOAT, SENS_ITF_DT_FLOAT, SENS_ITF_DT_U8, 
        SENS_ITF_DT_U8, SENS_ITF_DT_U32};
    uint8_t access_rights[SENS_ITF_POINT_NAME_SIZE] = { SENS_ITF_ACCESS_READ_ONLY, SENS_ITF_ACCESS_READ_ONLY, 
        SENS_ITF_ACCESS_READ_ONLY, SENS_ITF_ACCESS_WRITE_ONLY, SENS_ITF_ACCESS_READ_WRITE};
    uint32_t sampling_time[SENS_ITF_POINT_NAME_SIZE] = {4*10, 4*30, 4*1, 0, 0};

	memset(&sensor_points, 0, sizeof(sensor_points));
	memset(&board_info, 0, sizeof(board_info));
	
    strcpy(board_info.model, "KL46Z");
    strcpy(board_info.manufactor, "TESLA");
    board_info.sensor_id = 0xDEADBEEF;
    board_info.hardware_revision = 0x01;
    board_info.num_of_points = SENS_ITF_SENSOR_NUM_OF_POINTS;
    board_info.cabalities = SENS_ITF_CAPABILITIES_DISPLAY |
        SENS_ITF_CAPABILITIES_WPAN_STATUS | 
        SENS_ITF_CAPABILITIES_BATTERY_STATUS;

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

uint8_t sens_itf_sensor_init(void)
{
    num_rx_bytes = 0;
    process_frame = 0;
    sens_itf_init_point_db();
    rx_trmout_timer = os_timer_create((os_timer_func) sens_itf_rx_tmrout_timer_func, 0, 200, 0, 0);

    return 1;
}

