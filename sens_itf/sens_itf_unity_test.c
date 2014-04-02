#include <string.h>
#include <stdint.h>
#include "sens_itf.h"
#include "../os/os_defs.h"
#include "../os/os_timer.h"
#include "sens_util.h"
#include "../util/buf_io.h"
#include "../util/crc16.h"
#include "../unity/unity.h"

#ifdef __CMD_DEBUG__

static sens_itf_cmd_req_t cmd_mote;
static sens_itf_cmd_res_t ans_mote;
static sens_itf_cmd_req_t cmd_sensor;
static sens_itf_cmd_res_t ans_sensor;
static uint8_t frame[SENS_ITF_MAX_FRAME_SIZE];
static uint8_t size_mote;
static uint8_t size_sensor;
static uint8_t cmd_req_size;
static uint8_t cmd_res_size;
static uint8_t cmd_number;

void setUp(void)
{
    memset(&cmd_mote, 0, sizeof(sens_itf_cmd_req_t));
    memset(&ans_mote, 0, sizeof(sens_itf_cmd_res_t));
    memset(&cmd_sensor, 0, sizeof(sens_itf_cmd_req_t));
    memset(&ans_sensor, 0, sizeof(sens_itf_cmd_res_t));
    memset(frame, 0, SENS_ITF_MAX_FRAME_SIZE);

    size_mote = 0;
    size_sensor = 0;
    cmd_req_size = 0;
    cmd_res_size = 0;
}
 
void tearDown(void)
{
}

static void test_decode_ans(uint8_t expect_size, uint8_t res_size, sens_itf_cmd_res_t *expect_ans, sens_itf_cmd_res_t *ans)
{
    TEST_ASSERT_EQUAL_UINT8(expect_size, res_size);
    TEST_ASSERT_EQUAL_UINT8(expect_ans->hdr.size, ans->hdr.size);
    TEST_ASSERT_EQUAL_UINT8(expect_ans->hdr.addr, ans->hdr.addr);
    TEST_ASSERT_EQUAL_UINT8(expect_ans->hdr.status, ans->hdr.status);
    TEST_ASSERT_EQUAL_UINT8(expect_ans->crc, ans->crc);
}

static void test_decode_req(uint8_t expect_size, uint8_t res_size, sens_itf_cmd_req_t *expect_req, sens_itf_cmd_req_t *req)
{
    TEST_ASSERT_EQUAL_UINT8(expect_size, res_size);
    TEST_ASSERT_EQUAL_UINT8(expect_req->hdr.size, req->hdr.size);
    TEST_ASSERT_EQUAL_UINT8(expect_req->hdr.addr, req->hdr.addr);
    TEST_ASSERT_EQUAL_UINT8(expect_req->crc, req->crc);
}

static void validate_point_value(sens_itf_cmd_point_t *req, sens_itf_cmd_point_t *ans)
{
    switch (req->type)
    {
    case SENS_ITF_DT_U8:
        TEST_ASSERT_EQUAL_UINT8(req->value.u8, ans->value.u8);
        break;
    case SENS_ITF_DT_S8:
        TEST_ASSERT_EQUAL_INT8(req->value.s8, ans->value.s8);
        break;
    case SENS_ITF_DT_U16:
        TEST_ASSERT_EQUAL_UINT16(req->value.u16, ans->value.u16);
        break;
    case SENS_ITF_DT_S16:
        TEST_ASSERT_EQUAL_INT16(req->value.s16, ans->value.s16);
        break;
    case SENS_ITF_DT_U32:
        TEST_ASSERT_EQUAL_UINT32(req->value.u32, ans->value.u32);
        break;
    case SENS_ITF_DT_S32:
        TEST_ASSERT_EQUAL_INT32(req->value.s32, ans->value.s32); 
        break;
    case SENS_ITF_DT_U64:
        TEST_ASSERT_EQUAL_UINT64(req->value.u64, ans->value.u64);
        break;
    case SENS_ITF_DT_S64:
        TEST_ASSERT_EQUAL_INT64(req->value.s64, ans->value.s64);
        break;
    case SENS_ITF_DT_FLOAT:
        TEST_ASSERT_EQUAL_FLOAT(req->value.fp32, ans->value.fp32);
        break;
    case SENS_ITF_DT_DOUBLE:
        TEST_ASSERT_EQUAL_DOUBLE(req->value.fp64, ans->.value.fp64);
        break;
    default:
        break;
    }
}

static void test_SENS_ITF_REGMAP_ITF_VERSION(void)
{
    uint8_t frame_mote[SENS_ITF_MAX_FRAME_SIZE]   = {2, SENS_ITF_REGMAP_ITF_VERSION, 0x00, 0x00};
    uint8_t frame_sensor[SENS_ITF_MAX_FRAME_SIZE] = {4, SENS_ITF_REGMAP_ITF_VERSION, SENS_ITF_ANS_OK, SENS_ITF_LATEST_VERSION, 0x00, 0x00};
    uint16_t crc;

    setUp();

    cmd_req_size = 4;
    cmd_res_size = 6;
    cmd_number = SENS_ITF_REGMAP_ITF_VERSION;

    // enconde command req
    crc = crc16_calc(frame_mote, 2);
    buf_io_put16_tl(crc, &frame_mote[2]);
	cmd_mote.hdr.addr = SENS_ITF_REGMAP_ITF_VERSION;
    size_mote = sens_itf_pack_cmd_req(&cmd_mote, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_req_size, size_mote);
    TEST_ASSERT_EQUAL_INT8_ARRAY(frame_mote, frame, size_mote);
    
    // decode command req
    size_sensor = sens_itf_unpack_cmd_req(&cmd_sensor, frame, size_mote);
    TEST_ASSERT_EQUAL_UINT8(cmd_req_size, size_sensor);
    test_decode_req(cmd_req_size, size_sensor,&cmd_mote, &cmd_sensor);

    // encode command res
    crc = crc16_calc(frame_sensor, 4);
    buf_io_put16_tl(crc, &frame_sensor[4]);
    ans_sensor.hdr.addr = SENS_ITF_REGMAP_ITF_VERSION;
    ans_sensor.hdr.status = SENS_ITF_ANS_OK;
    ans_sensor.payload.itf_version_cmd.version = SENS_ITF_LATEST_VERSION;
    size_sensor = sens_itf_pack_cmd_res(&ans_sensor, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_res_size, size_sensor);
    TEST_ASSERT_EQUAL_INT8_ARRAY(frame_sensor, frame, size_sensor);

    // decode command res
    size_mote = sens_itf_unpack_cmd_res(&ans_mote, frame, size_sensor);
    TEST_ASSERT_EQUAL_UINT8(cmd_res_size, size_mote);
    test_decode_ans(cmd_res_size, size_mote,&ans_sensor,&ans_mote);
    TEST_ASSERT_EQUAL_UINT8(ans_mote.payload.itf_version_cmd.version, ans_sensor.payload.itf_version_cmd.version);
    
}

static void test_SENS_ITF_REGMAP_BRD_ID(void)
{
    setUp();

    cmd_req_size = 4;
    cmd_res_size = 28;
    cmd_number = SENS_ITF_REGMAP_BRD_ID;

    // enconde command req
    cmd_mote.hdr.addr = cmd_number;
    size_mote = sens_itf_pack_cmd_req(&cmd_mote, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_req_size, size_mote);

    // decode command req
    size_sensor = sens_itf_unpack_cmd_req(&cmd_sensor, frame, size_mote);
    test_decode_req(cmd_req_size, size_sensor,&cmd_mote, &cmd_sensor);

    // encode command res
    ans_sensor.hdr.addr = cmd_number;
    ans_sensor.hdr.status = SENS_ITF_ANS_OK;
    strcpy(ans_sensor.payload.brd_id_cmd.model, "??#KL46Z");
    strcpy(ans_sensor.payload.brd_id_cmd.manufactor, "TESLA");
    ans_sensor.payload.brd_id_cmd.sensor_id = 0xDEADBEEF;
    ans_sensor.payload.brd_id_cmd.hardware_revision = 0x01;
    ans_sensor.payload.brd_id_cmd.num_of_points = 5;
    ans_sensor.payload.brd_id_cmd.cabalities = SENS_ITF_CAPABILITIES_DISPLAY |
        SENS_ITF_CAPABILITIES_WPAN_STATUS | 
        SENS_ITF_CAPABILITIES_BATTERY_STATUS;
    size_sensor = sens_itf_pack_cmd_res(&ans_sensor, frame);
    TEST_ASSERT_EQUAL_INT(cmd_res_size, size_sensor);

    // decode command res
    size_mote = sens_itf_unpack_cmd_res(&ans_mote, frame, size_sensor);
    test_decode_ans(cmd_res_size, size_mote,&ans_sensor,&ans_mote);
    TEST_ASSERT_EQUAL_INT8_ARRAY(ans_sensor.payload.brd_id_cmd.model, ans_mote.payload.brd_id_cmd.model,SENS_ITF_MODEL_NAME_SIZE);
    TEST_ASSERT_EQUAL_INT8_ARRAY(ans_sensor.payload.brd_id_cmd.manufactor, ans_mote.payload.brd_id_cmd.manufactor,SENS_ITF_MANUF_NAME_SIZE);
    TEST_ASSERT_EQUAL_INT32(ans_sensor.payload.brd_id_cmd.sensor_id,ans_mote.payload.brd_id_cmd.sensor_id);
    TEST_ASSERT_EQUAL_INT32(ans_sensor.payload.brd_id_cmd.sensor_id,ans_mote.payload.brd_id_cmd.sensor_id);
    TEST_ASSERT_EQUAL_INT32(ans_sensor.payload.brd_id_cmd.sensor_id,ans_mote.payload.brd_id_cmd.sensor_id);
    TEST_ASSERT_EQUAL_UINT8(ans_sensor.payload.brd_id_cmd.hardware_revision,ans_mote.payload.brd_id_cmd.hardware_revision);
    TEST_ASSERT_EQUAL_UINT8(ans_sensor.payload.brd_id_cmd.num_of_points,ans_mote.payload.brd_id_cmd.num_of_points);
    TEST_ASSERT_EQUAL_UINT8(ans_sensor.payload.brd_id_cmd.cabalities,ans_mote.payload.brd_id_cmd.cabalities);
}

static void test_SENS_ITF_REGMAP_BRD_STATUS(void)
{
    setUp();

    cmd_req_size = 4;
    cmd_res_size = 6;
    cmd_number = SENS_ITF_REGMAP_BRD_STATUS;

    // enconde command req
    cmd_mote.hdr.addr = cmd_number;
    size_mote = sens_itf_pack_cmd_req(&cmd_mote, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_req_size, size_mote);

    // decode command req
    size_sensor = sens_itf_unpack_cmd_req(&cmd_sensor, frame, size_mote);
    test_decode_req(cmd_req_size, size_sensor,&cmd_mote, &cmd_sensor);

    // encode command res
    ans_sensor.hdr.addr = cmd_number;
    ans_sensor.hdr.status = SENS_ITF_ANS_OK;
    ans_sensor.payload.bat_status_cmd.status = 0xff;
    size_sensor = sens_itf_pack_cmd_res(&ans_sensor, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_res_size, size_sensor);

    // decode command res
    size_mote = sens_itf_unpack_cmd_res(&ans_mote, frame, size_sensor);
    test_decode_ans(cmd_res_size, size_mote,&ans_sensor,&ans_mote);
    TEST_ASSERT_EQUAL_UINT8(ans_mote.payload.brd_status_cmd.status, ans_sensor.payload.brd_status_cmd.status);
}

static void test_SENS_ITF_REGMAP_BRD_CMD(void)
{
    setUp();

    cmd_req_size = 5;
    cmd_res_size = 6;
    cmd_number = SENS_ITF_REGMAP_BRD_CMD;

    // enconde command req
    cmd_mote.hdr.addr = cmd_number;
    cmd_mote.payload.command_cmd.cmd = 0xff;
    size_mote = sens_itf_pack_cmd_req(&cmd_mote, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_req_size, size_mote);

    // decode command req
    size_sensor = sens_itf_unpack_cmd_req(&cmd_sensor, frame, size_mote);
    test_decode_req(cmd_req_size, size_sensor,&cmd_mote, &cmd_sensor);
    TEST_ASSERT_EQUAL_UINT8(cmd_mote.payload.command_cmd.cmd, cmd_sensor.payload.command_cmd.cmd);

    // encode command res
    ans_sensor.hdr.addr = cmd_number;
    ans_sensor.hdr.status = SENS_ITF_ANS_OK;
    ans_sensor.payload.command_res_cmd.status = 0xff;
    size_sensor = sens_itf_pack_cmd_res(&ans_sensor, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_res_size, size_sensor);

    // decode command res
    size_mote = sens_itf_unpack_cmd_res(&ans_mote, frame, size_sensor);
    test_decode_ans(cmd_res_size, size_mote,&ans_sensor,&ans_mote);
    TEST_ASSERT_EQUAL_UINT8(ans_mote.payload.command_res_cmd.status, ans_sensor.payload.command_res_cmd.status);
}
static void test_SENS_ITF_REGMAP_READ_BAT_STATUS(void)
{
    setUp();

    cmd_req_size = 4;
    cmd_res_size = 6;
    cmd_number = SENS_ITF_REGMAP_READ_BAT_STATUS;

    // enconde command req
    cmd_mote.hdr.addr = cmd_number;
    size_mote = sens_itf_pack_cmd_req(&cmd_mote, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_req_size, size_mote);

    // decode command req
    size_sensor = sens_itf_unpack_cmd_req(&cmd_sensor, frame, size_mote);
    test_decode_req(cmd_req_size, size_sensor,&cmd_mote, &cmd_sensor);

    // encode command res
    ans_sensor.hdr.addr = cmd_number;
    ans_sensor.hdr.status = SENS_ITF_ANS_OK;
    ans_sensor.payload.bat_status_cmd.status = 0xff;
    size_sensor = sens_itf_pack_cmd_res(&ans_sensor, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_res_size, size_sensor);

    // decode command res
    size_mote = sens_itf_unpack_cmd_res(&ans_mote, frame, size_sensor);
    test_decode_ans(cmd_res_size, size_mote,&ans_sensor,&ans_mote);
    TEST_ASSERT_EQUAL_UINT8(ans_mote.payload.bat_status_cmd.status, ans_sensor.payload.bat_status_cmd.status);
}


static void test_SENS_ITF_REGMAP_WRITE_BAT_STATUS(void)
{
    cmd_req_size = 5;
    cmd_res_size = 5;
    cmd_number = SENS_ITF_REGMAP_WRITE_BAT_STATUS;

    // enconde command req
    cmd_mote.hdr.addr = cmd_number;
    cmd_mote.payload.bat_status_cmd.status = 0x22;
    size_mote = sens_itf_pack_cmd_req(&cmd_mote, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_req_size, size_mote);

    // decode command req
    size_sensor = sens_itf_unpack_cmd_req(&cmd_sensor, frame, size_mote);
    test_decode_req(cmd_req_size, size_sensor,&cmd_mote, &cmd_sensor);
    TEST_ASSERT_EQUAL_UINT8(cmd_mote.payload.bat_status_cmd.status, cmd_sensor.payload.bat_status_cmd.status);
    
    // encode command res
    ans_sensor.hdr.addr = cmd_number;
    ans_sensor.hdr.status = SENS_ITF_ANS_OK;
    size_sensor = sens_itf_pack_cmd_res(&ans_sensor, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_res_size, size_sensor);

    // decode command res
    size_mote = sens_itf_unpack_cmd_res(&ans_mote, frame, size_sensor);
    test_decode_ans(cmd_res_size, size_mote,&ans_sensor,&ans_mote);
}
static void test_SENS_ITF_REGMAP_READ_BAT_CHARGE(void)
{
    cmd_req_size = 4;
    cmd_res_size = 6;
    cmd_number = SENS_ITF_REGMAP_READ_BAT_CHARGE;

    // enconde command req
    cmd_mote.hdr.addr = cmd_number;
    size_mote = sens_itf_pack_cmd_req(&cmd_mote, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_req_size, size_mote);

    // decode command req
    size_sensor = sens_itf_unpack_cmd_req(&cmd_sensor, frame, size_mote);
    test_decode_req(cmd_req_size, size_sensor,&cmd_mote, &cmd_sensor);
    
    // encode command res
    ans_sensor.hdr.addr = cmd_number;
    ans_sensor.hdr.status = SENS_ITF_ANS_OK;
    ans_sensor.payload.bat_charge_cmd.charge = 50;
    size_sensor = sens_itf_pack_cmd_res(&ans_sensor, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_res_size, size_sensor);

    // decode command res
    size_mote = sens_itf_unpack_cmd_res(&ans_mote, frame, size_sensor);
    test_decode_ans(cmd_res_size, size_mote,&ans_sensor,&ans_mote);
    TEST_ASSERT_EQUAL_UINT8(ans_mote.payload.bat_charge_cmd.charge, ans_sensor.payload.bat_charge_cmd.charge);
}

static void test_SENS_ITF_REGMAP_WRITE_BAT_CHARGE(void)
{
    cmd_req_size = 5;
    cmd_res_size = 5;
    cmd_number = SENS_ITF_REGMAP_WRITE_BAT_CHARGE;

    // enconde command req
    cmd_mote.hdr.addr = cmd_number;
    cmd_mote.payload.bat_charge_cmd.charge = 50;
    size_mote = sens_itf_pack_cmd_req(&cmd_mote, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_req_size, size_mote);

    // decode command req
    size_sensor = sens_itf_unpack_cmd_req(&cmd_sensor, frame, size_mote);
    test_decode_req(cmd_req_size, size_sensor,&cmd_mote, &cmd_sensor);
    TEST_ASSERT_EQUAL_UINT8(cmd_mote.payload.bat_charge_cmd.charge, cmd_sensor.payload.bat_charge_cmd.charge);
    
    // encode command res
    ans_sensor.hdr.addr = cmd_number;
    ans_sensor.hdr.status = SENS_ITF_ANS_OK;
    size_sensor = sens_itf_pack_cmd_res(&ans_sensor, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_res_size, size_sensor);

    // decode command res
    size_mote = sens_itf_unpack_cmd_res(&ans_mote, frame, size_sensor);
    test_decode_ans(cmd_res_size, size_mote,&ans_sensor,&ans_mote);
}

static void test_SENS_ITF_REGMAP_WPAN_STATUS(void)
{
    cmd_req_size = 5;
    cmd_res_size = 5;
    cmd_number = SENS_ITF_REGMAP_WPAN_STATUS;

    // enconde command req
    cmd_mote.hdr.addr = cmd_number;
    cmd_mote.payload.wpan_status_cmd.status = 50;
    size_mote = sens_itf_pack_cmd_req(&cmd_mote, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_req_size, size_mote);

    // decode command req
    size_sensor = sens_itf_unpack_cmd_req(&cmd_sensor, frame, size_mote);
    test_decode_req(cmd_req_size, size_sensor,&cmd_mote, &cmd_sensor);
    TEST_ASSERT_EQUAL_UINT8(cmd_mote.payload.wpan_status_cmd.status, cmd_sensor.payload.wpan_status_cmd.status);
    
    // encode command res
    ans_sensor.hdr.addr = cmd_number;
    ans_sensor.hdr.status = SENS_ITF_ANS_OK;
    size_sensor = sens_itf_pack_cmd_res(&ans_sensor, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_res_size, size_sensor);

    // decode command res
    size_mote = sens_itf_unpack_cmd_res(&ans_mote, frame, size_sensor);
    test_decode_ans(cmd_res_size, size_mote,&ans_sensor,&ans_mote);
}

static void test_SENS_ITF_REGMAP_WPAN_STRENGTH(void)
{
    cmd_req_size = 5;
    cmd_res_size = 5;
    cmd_number = SENS_ITF_REGMAP_WPAN_STRENGTH;

    // enconde command req
    cmd_mote.hdr.addr = cmd_number;
    cmd_mote.payload.wpan_strength_cmd.strenght = 50;
    size_mote = sens_itf_pack_cmd_req(&cmd_mote, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_req_size, size_mote);

    // decode command req
    size_sensor = sens_itf_unpack_cmd_req(&cmd_sensor, frame, size_mote);
    test_decode_req(cmd_req_size, size_sensor,&cmd_mote, &cmd_sensor);
    TEST_ASSERT_EQUAL_UINT8(cmd_mote.payload.wpan_strength_cmd.strenght, cmd_sensor.payload.wpan_strength_cmd.strenght);
    
    // encode command res
    ans_sensor.hdr.addr = cmd_number;
    ans_sensor.hdr.status = SENS_ITF_ANS_OK;
    size_sensor = sens_itf_pack_cmd_res(&ans_sensor, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_res_size, size_sensor);

    // decode command res
    size_mote = sens_itf_unpack_cmd_res(&ans_mote, frame, size_sensor);
    test_decode_ans(cmd_res_size, size_mote,&ans_sensor,&ans_mote);
}

static void test_SENS_ITF_REGMAP_DSP_WRITE(void)
{
    cmd_req_size = 29;
    cmd_res_size = 5;
    cmd_number = SENS_ITF_REGMAP_DSP_WRITE;

    // enconde command req
    cmd_mote.hdr.addr = cmd_number;
    cmd_mote.payload.write_display_cmd.line = 1;
    strcpy(cmd_mote.payload.write_display_cmd.msg, "WPAN FOUND !");
    size_mote = sens_itf_pack_cmd_req(&cmd_mote, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_req_size, size_mote);

    // decode command req
    size_sensor = sens_itf_unpack_cmd_req(&cmd_sensor, frame, size_mote);
    test_decode_req(cmd_req_size, size_sensor,&cmd_mote, &cmd_sensor);
    TEST_ASSERT_EQUAL_INT8_ARRAY(cmd_mote.payload.write_display_cmd.msg, cmd_mote.payload.write_display_cmd.msg,SENS_ITF_DSP_MSG_MAX_SIZE);
    TEST_ASSERT_EQUAL_UINT8(cmd_mote.payload.write_display_cmd.line, cmd_mote.payload.write_display_cmd.line);

    // encode command res
    ans_sensor.hdr.addr = cmd_number;
    ans_sensor.hdr.status = SENS_ITF_ANS_OK;
    size_sensor = sens_itf_pack_cmd_res(&ans_sensor, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_res_size, size_sensor);

    // decode command res
    size_mote = sens_itf_unpack_cmd_res(&ans_mote, frame, size_sensor);
    test_decode_ans(cmd_res_size, size_mote,&ans_sensor,&ans_mote);
}

static void test_SENS_ITF_REGMAP_SVR_MAIN_ADDR(void)
{
    cmd_req_size = 4;
    cmd_res_size = 21;
    cmd_number = SENS_ITF_REGMAP_SVR_MAIN_ADDR;

    // enconde command req
    cmd_mote.hdr.addr = cmd_number;
    size_mote = sens_itf_pack_cmd_req(&cmd_mote, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_req_size, size_mote);

    // decode command req
    size_sensor = sens_itf_unpack_cmd_req(&cmd_sensor, frame, size_mote);
    test_decode_req(cmd_req_size, size_sensor,&cmd_mote, &cmd_sensor);

    // encode command res
    ans_sensor.hdr.addr = cmd_number;
    ans_sensor.hdr.status = SENS_ITF_ANS_OK;
    memcpy(ans_sensor.payload.svr_addr_cmd.addr, "1234567812345678", 16);
    size_sensor = sens_itf_pack_cmd_res(&ans_sensor, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_res_size, size_sensor);

    // decode command res
    size_mote = sens_itf_unpack_cmd_res(&ans_mote, frame, size_sensor);
    test_decode_ans(cmd_res_size, size_mote,&ans_sensor,&ans_mote);
    TEST_ASSERT_EQUAL_INT8_ARRAY(ans_sensor.payload.svr_addr_cmd.addr,ans_mote.payload.svr_addr_cmd.addr,16);
}

static void test_SENS_ITF_REGMAP_SVR_SEC_ADDR(void)
{
    cmd_req_size = 4;
    cmd_res_size = 21;
    cmd_number = SENS_ITF_REGMAP_SVR_SEC_ADDR;

    // enconde command req
    cmd_mote.hdr.addr = cmd_number;
    size_mote = sens_itf_pack_cmd_req(&cmd_mote, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_req_size, size_mote);

    // decode command req
    size_sensor = sens_itf_unpack_cmd_req(&cmd_sensor, frame, size_mote);
    test_decode_req(cmd_req_size, size_sensor,&cmd_mote, &cmd_sensor);

    // encode command res
    ans_sensor.hdr.addr = cmd_number;
    ans_sensor.hdr.status = SENS_ITF_ANS_OK;
    memcpy(ans_sensor.payload.svr_addr_cmd.addr, "1234567812345678", 16);
    size_sensor = sens_itf_pack_cmd_res(&ans_sensor, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_res_size, size_sensor);

    // decode command res
    size_mote = sens_itf_unpack_cmd_res(&ans_mote, frame, size_sensor);
    test_decode_ans(cmd_res_size, size_mote,&ans_sensor,&ans_mote);
    TEST_ASSERT_EQUAL_INT8_ARRAY(ans_sensor.payload.svr_addr_cmd.addr,ans_mote.payload.svr_addr_cmd.addr,16);}

static void test_SENS_ITF_REGMAP_POINT_DESC_1(void)
{
    cmd_req_size = 4;
    cmd_res_size = 20;
    cmd_number = SENS_ITF_REGMAP_POINT_DESC_1;

    // enconde command req
    cmd_mote.hdr.addr = cmd_number;
    size_mote = sens_itf_pack_cmd_req(&cmd_mote, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_req_size, size_mote);

    // decode command req
    size_sensor = sens_itf_unpack_cmd_req(&cmd_sensor, frame, size_mote);
    test_decode_req(cmd_req_size, size_sensor,&cmd_mote, &cmd_sensor);
    
    // encode command res
    ans_sensor.hdr.addr = cmd_number;
    ans_sensor.hdr.status = SENS_ITF_ANS_OK;
    strcpy(ans_sensor.payload.point_desc_cmd.name, "TEMP");
    ans_sensor.payload.point_desc_cmd.type = SENS_ITF_DT_U8;
    ans_sensor.payload.point_desc_cmd.unit = 0; // TBD
    ans_sensor.payload.point_desc_cmd.access_rights = SENS_ITF_ACCESS_READ_ONLY;
    ans_sensor.payload.point_desc_cmd.sampling_time_x250ms = 4 * 10;
    size_sensor = sens_itf_pack_cmd_res(&ans_sensor, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_res_size, size_sensor);

    // decode command res
    size_mote = sens_itf_unpack_cmd_res(&ans_mote, frame, size_sensor);
    test_decode_ans(cmd_res_size, size_mote,&ans_sensor,&ans_mote);
    TEST_ASSERT_EQUAL_INT8_ARRAY(ans_sensor.payload.point_desc_cmd.name, ans_mote.payload.point_desc_cmd.name,SENS_ITF_POINT_NAME_SIZE);
    TEST_ASSERT_EQUAL_UINT8(ans_sensor.payload.point_desc_cmd.type,ans_mote.payload.point_desc_cmd.type);
    TEST_ASSERT_EQUAL_UINT8(ans_sensor.payload.point_desc_cmd.unit,ans_mote.payload.point_desc_cmd.unit);
    TEST_ASSERT_EQUAL_UINT8(ans_sensor.payload.point_desc_cmd.access_rights,ans_mote.payload.point_desc_cmd.access_rights);
    TEST_ASSERT_EQUAL_UINT8(ans_sensor.payload.point_desc_cmd.sampling_time_x250ms,ans_mote.payload.point_desc_cmd.sampling_time_x250ms);
}

static void test_SENS_ITF_REGMAP_POINT_DESC_2(void)
{
    cmd_req_size = 4;
    cmd_res_size = 20;
    cmd_number = SENS_ITF_REGMAP_POINT_DESC_2;

    // enconde command req
    cmd_mote.hdr.addr = cmd_number;
    size_mote = sens_itf_pack_cmd_req(&cmd_mote, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_req_size, size_mote);

    // decode command req
    size_sensor = sens_itf_unpack_cmd_req(&cmd_sensor, frame, size_mote);
    test_decode_req(cmd_req_size, size_sensor,&cmd_mote, &cmd_sensor);
    
    // encode command res
    ans_sensor.hdr.addr = cmd_number;
    ans_sensor.hdr.status = SENS_ITF_ANS_OK;
    memcpy(ans_sensor.payload.point_desc_cmd.name, "12345678",SENS_ITF_POINT_NAME_SIZE);
    ans_sensor.payload.point_desc_cmd.type = SENS_ITF_DT_FLOAT;
    ans_sensor.payload.point_desc_cmd.unit = 10; // TBD
    ans_sensor.payload.point_desc_cmd.access_rights = SENS_ITF_ACCESS_READ_WRITE;
    ans_sensor.payload.point_desc_cmd.sampling_time_x250ms = 4 * 100;
    size_sensor = sens_itf_pack_cmd_res(&ans_sensor, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_res_size, size_sensor);

    // decode command res
    size_mote = sens_itf_unpack_cmd_res(&ans_mote, frame, size_sensor);
    test_decode_ans(cmd_res_size, size_mote,&ans_sensor,&ans_mote);
    TEST_ASSERT_EQUAL_INT8_ARRAY(ans_sensor.payload.point_desc_cmd.name, ans_mote.payload.point_desc_cmd.name,SENS_ITF_POINT_NAME_SIZE);
    TEST_ASSERT_EQUAL_UINT8(ans_sensor.payload.point_desc_cmd.type,ans_mote.payload.point_desc_cmd.type);
    TEST_ASSERT_EQUAL_UINT8(ans_sensor.payload.point_desc_cmd.unit,ans_mote.payload.point_desc_cmd.unit);
    TEST_ASSERT_EQUAL_UINT8(ans_sensor.payload.point_desc_cmd.access_rights,ans_mote.payload.point_desc_cmd.access_rights);
    TEST_ASSERT_EQUAL_UINT8(ans_sensor.payload.point_desc_cmd.sampling_time_x250ms,ans_mote.payload.point_desc_cmd.sampling_time_x250ms);
}

void test_SENS_ITF_REGMAP_READ_POINT_DATA_1(void)
{
    cmd_req_size = 4;
    cmd_res_size = 10;
    cmd_number = SENS_ITF_REGMAP_READ_POINT_DATA_1;

    // enconde command req
    cmd_mote.hdr.addr = cmd_number;
    size_mote = sens_itf_pack_cmd_req(&cmd_mote, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_req_size, size_mote);

    // decode command req
    size_sensor = sens_itf_unpack_cmd_req(&cmd_sensor, frame, size_mote);
    test_decode_req(cmd_req_size, size_sensor,&cmd_mote, &cmd_sensor);
    
    // encode command res
    ans_sensor.hdr.addr = cmd_number;
    ans_sensor.hdr.status = SENS_ITF_ANS_OK;
    ans_sensor.payload.point_value_cmd.type = SENS_ITF_DT_FLOAT;
    ans_sensor.payload.point_value_cmd.value.fp32 = 3.141592f;
    size_sensor = sens_itf_pack_cmd_res(&ans_sensor, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_res_size, size_sensor);

    // decode command res
    size_mote = sens_itf_unpack_cmd_res(&ans_mote, frame, size_sensor);
    test_decode_ans(cmd_res_size, size_mote,&ans_sensor,&ans_mote);
    validate_point_value(&ans_sensor.payload.point_value_cmd, &ans_mote.payload.point_value_cmd);
}

void test_SENS_ITF_REGMAP_WRITE_POINT_DATA_5(void)
{
    cmd_req_size = 9;
    cmd_res_size = 5;
    cmd_number = SENS_ITF_REGMAP_WRITE_POINT_DATA_5;

    // enconde command req
    cmd_mote.hdr.addr = cmd_number;
    cmd_mote.payload.point_value_cmd.type = SENS_ITF_DT_U32;
    cmd_mote.payload.point_value_cmd.value.u32 = 0x12345678;
    size_mote = sens_itf_pack_cmd_req(&cmd_mote, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_req_size, size_mote);

    // decode command req
    size_sensor = sens_itf_unpack_cmd_req(&cmd_sensor, frame, size_mote);
    test_decode_req(cmd_req_size, size_sensor,&cmd_mote, &cmd_sensor);
    validate_point_value(&cmd_mote.payload.point_value_cmd, &cmd_sensor.payload.point_value_cmd);
    
    // encode command res
    ans_sensor.hdr.addr = cmd_number;
    ans_sensor.hdr.status = SENS_ITF_ANS_OK;
    size_sensor = sens_itf_pack_cmd_res(&ans_sensor, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_res_size, size_sensor);

    // decode command res
    size_mote = sens_itf_unpack_cmd_res(&ans_mote, frame, size_sensor);
    test_decode_ans(cmd_res_size, size_mote,&ans_sensor,&ans_mote);
}

void test_SENS_ITF_REGMAP_READ_POINT_DATA_32(void)
{
    cmd_req_size = 4;
    cmd_res_size = 8;
    cmd_number = SENS_ITF_REGMAP_READ_POINT_DATA_32;

    // enconde command req
    cmd_mote.hdr.addr = cmd_number;
    size_mote = sens_itf_pack_cmd_req(&cmd_mote, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_req_size, size_mote);

    // decode command req
    size_sensor = sens_itf_unpack_cmd_req(&cmd_sensor, frame, size_mote);
    test_decode_req(cmd_req_size, size_sensor,&cmd_mote, &cmd_sensor);
    
    // encode command res
    ans_sensor.hdr.addr = cmd_number;
    ans_sensor.hdr.status = SENS_ITF_ANS_OK;
    ans_sensor.payload.point_value_cmd.type = SENS_ITF_DT_S16;
    ans_sensor.payload.point_value_cmd.value.s16 = -512;
    size_sensor = sens_itf_pack_cmd_res(&ans_sensor, frame);
    TEST_ASSERT_EQUAL_UINT8(cmd_res_size, size_sensor);

    // decode command res
    size_mote = sens_itf_unpack_cmd_res(&ans_mote, frame, size_sensor);
    test_decode_ans(cmd_res_size, size_mote,&ans_sensor,&ans_mote);
    validate_point_value(&ans_sensor.payload.point_value_cmd, &ans_mote.payload.point_value_cmd);
}

void test_main(void)
{
    UnityBegin();

    RUN_TEST(test_SENS_ITF_REGMAP_ITF_VERSION,__LINE__);
    RUN_TEST(test_SENS_ITF_REGMAP_BRD_ID,__LINE__);
    RUN_TEST(test_SENS_ITF_REGMAP_BRD_STATUS,__LINE__);
    RUN_TEST(test_SENS_ITF_REGMAP_BRD_CMD,__LINE__);
    RUN_TEST(test_SENS_ITF_REGMAP_READ_BAT_STATUS,__LINE__);
    RUN_TEST(test_SENS_ITF_REGMAP_WRITE_BAT_STATUS,__LINE__);
    RUN_TEST(test_SENS_ITF_REGMAP_READ_BAT_CHARGE,__LINE__);
    RUN_TEST(test_SENS_ITF_REGMAP_WRITE_BAT_CHARGE,__LINE__);
    RUN_TEST(test_SENS_ITF_REGMAP_WPAN_STATUS,__LINE__);
    RUN_TEST(test_SENS_ITF_REGMAP_WPAN_STRENGTH,__LINE__);
    RUN_TEST(test_SENS_ITF_REGMAP_DSP_WRITE,__LINE__);
    RUN_TEST(test_SENS_ITF_REGMAP_SVR_MAIN_ADDR,__LINE__);
    RUN_TEST(test_SENS_ITF_REGMAP_SVR_SEC_ADDR,__LINE__);
    RUN_TEST(test_SENS_ITF_REGMAP_POINT_DESC_1,__LINE__);
    RUN_TEST(test_SENS_ITF_REGMAP_POINT_DESC_2,__LINE__);
    RUN_TEST(test_SENS_ITF_REGMAP_READ_POINT_DATA_1,__LINE__);
    RUN_TEST(test_SENS_ITF_REGMAP_WRITE_POINT_DATA_5,__LINE__);
    RUN_TEST(test_SENS_ITF_REGMAP_READ_POINT_DATA_32,__LINE__);
    
    UnityEnd();
}

int main(void)
{
    sens_util_log_start();
    sens_itf_sensor_init();
    sens_itf_mote_init();

    test_main();

    sens_util_log_stop();
    
    return 0;
}

#endif


/*


    cmd_req_size = ;
    cmd_res_size = ;
    cmd_number = ;

    // enconde command req
    cmd_mote.hdr.addr = cmd_number;
    size_mote = sens_itf_pack_cmd_req(&cmd_mote, frame);
    TEST_ASSERT_EQUAL_INT(cmd_req_size, size_mote);

    // decode command req
    size_sensor = sens_itf_unpack_cmd_req(&cmd_sensor, frame, size_mote);
    test_decode_req(cmd_req_size, size_sensor,&cmd_mote, &cmd_sensor);
    
    // encode command res
    ans_sensor.hdr.addr = cmd_number;
    ans_sensor.hdr.status = SENS_ITF_ANS_OK;
    size_sensor = sens_itf_pack_cmd_res(&ans_sensor, frame);
    TEST_ASSERT_EQUAL_INT(cmd_res_size, size_sensor);

    // decode command res
    size_mote = sens_itf_unpack_cmd_res(&ans_mote, frame, size_sensor);
    test_decode_ans(cmd_res_size, size_mote,&ans_sensor,&ans_mote);

*/