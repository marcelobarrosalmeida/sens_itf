#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "osens.h"
#include "osens_itf.h"
#include "../os/os_kernel.h"
#include "../os/os_serial.h"
#include "../os/os_util.h"

#define TRACE_ON 1

// minimum tick time is 1
#define OSENS_SM_TICK_MS 250

#define MS2TICK(ms) (ms) > OSENS_SM_TICK_MS ? (ms) / OSENS_SM_TICK_MS : 1

enum {
    OSENS_STATE_INIT = 0,
    OSENS_STATE_SEND_ITF_VER = 1,
    OSENS_STATE_WAIT_ITF_VER_ANS = 2,
    OSENS_STATE_PROC_ITF_VER = 3,
    OSENS_STATE_SEND_BRD_ID = 4,
    OSENS_STATE_WAIT_BRD_ID_ANS = 5,
    OSENS_STATE_PROC_BRD_ID = 6,
    OSENS_STATE_SEND_PT_DESC = 7,
    OSENS_STATE_WAIT_PT_DESC_ANS = 8,
    OSENS_STATE_PROC_PT_DESC = 9,
    OSENS_STATE_BUILD_SCH = 10,
    OSENS_STATE_RUN_SCH = 11,
    OSENS_STATE_SEND_PT_VAL = 12,
    OSENS_STATE_WAIT_PT_VAL_ANS = 13,
    OSENS_STATE_PROC_PT_VAL = 14
};

uint8_t *sm_states_str[] = {
    "INIT",
    "SEND_ITF_VER",
    "WAIT_ITF_VER_ANS",
    "PROC_ITF_VER",
    "SEND_BRD_ID",
    "WAIT_BRD_ID_ANS",
    "PROC_BRD_ID",
    "SEND_PT_DESC",
    "WAIT_PT_DESC_ANS",
    "PROC_PT_DESC",
    "BUILD_SCH",
    "RUN_SCH",
    "SEND_PT_VAL",
    "WAIT_PT_VAL_ANS",
    "PROC_PT_VAL"
};
enum {
    OSENS_STATE_EXEC_OK = 0,
    OSENS_STATE_EXEC_WAIT_OK,
    OSENS_STATE_EXEC_WAIT_STOP,
    OSENS_STATE_EXEC_WAIT_ABORT,
    OSENS_STATE_EXEC_ERROR
};

typedef struct osens_mote_sm_state_s
{
    volatile uint16_t trmout_counter;
    volatile uint16_t trmout;
    volatile uint8_t point_index;
    volatile uint8_t frame_arrived;
    volatile uint8_t state;
    volatile uint8_t retries;
} osens_mote_sm_state_t;

typedef uint8_t(*osens_mote_sm_func_t)(osens_mote_sm_state_t *st);

typedef struct osens_mote_sm_table_s
{
    osens_mote_sm_func_t func;
    uint8_t next_state;
    uint8_t abort_state; // for indicating timeout or end of cyclic operation
    uint8_t error_state;
} osens_mote_sm_table_t;

typedef struct osens_acq_schedule_s
{
    uint8_t num_of_points;
    struct
    {
        uint8_t index;
        uint32_t sampling_time_x250ms;
        uint32_t counter;
    } points[OSENS_MAX_POINTS];

    struct
    {
        uint8_t num_of_points;
        uint8_t index[OSENS_MAX_POINTS];
    } scan;
} osens_acq_schedule_t;

static uint8_t osens_mote_sm_func_build_sch(osens_mote_sm_state_t *st);
static uint8_t osens_mote_sm_func_pt_desc_ans(osens_mote_sm_state_t *st);
static uint8_t osens_mote_sm_func_req_pt_desc(osens_mote_sm_state_t *st);
static uint8_t osens_mote_sm_func_run_sch(osens_mote_sm_state_t *st);

const osens_mote_sm_table_t osens_mote_sm_table[];
const uint8_t datatype_sizes[] = { 1, 1, 2, 2, 4, 4, 8, 8, 4, 8 }; // check osens_datatypes_e order

uint8_t num_rx_bytes;
uint8_t tx_data_len;
uint32_t flagErrorOccurred;

uint8_t frame[OSENS_MAX_FRAME_SIZE];

osens_cmd_req_t cmd;
osens_cmd_res_t ans;

osens_mote_sm_state_t sm_state;
osens_point_ctrl_t sensor_points;
osens_brd_id_t board_info;
osens_acq_schedule_t acquisition_schedule;

static os_thread_t sm_thread;
static os_thread_t rx_thread;
static os_serial_t serial = 0;
static volatile uint64_t tick_counter;

//=========================== prototypes =======================================
//=========================== public ==========================================
void sensor_timer(void);
static void buBufFlush(void);
void bspLedToggle(uint8_t ui8Leds);
void osens_mote_sm(void);

static void* osens_mote_tick(void* param)
{
    //scheduler_push_task(osens_mote_sm, TASKPRIO_OSENS_MAIN);
    while (1)
    {
        tick_counter++;
        osens_mote_sm();
        os_kernel_sleep(OSENS_SM_TICK_MS);
    }
    return 0;
}

uint8_t osens_mote_send_frame(uint8_t *frame, uint8_t size)
{
    int16_t sent;
#if OSENS_DBG_FRAME == 1
    os_util_dump_frame(frame, size);
#endif
    os_serial_flush(serial);
    sent = os_serial_write(serial, frame, size);
    return (sent < 0 ? 0 : (uint8_t) sent); // CHECK AGAIN
}

#if TRACE_ON
static void osens_mote_show_values(void)
{
    uint8_t n;
    OS_UTIL_LOG(1, ("\n"));
    for (n = 0; n < sensor_points.num_of_points; n++)
    {
        switch (sensor_points.points[n].value.type)
        {
        case OSENS_DT_U8:
            OS_UTIL_LOG(1, ("Point %02d: %d\n", n, sensor_points.points[n].value.value.u8));
            break;
        case OSENS_DT_S8:
            OS_UTIL_LOG(1, ("Point %02d: %d\n", n, sensor_points.points[n].value.value.s8));
            break;
        case OSENS_DT_U16:
            OS_UTIL_LOG(1, ("Point %02d: %d\n", n, sensor_points.points[n].value.value.u16));
            break;
        case OSENS_DT_S16:
            OS_UTIL_LOG(1, ("Point %02d: %d\n", n, sensor_points.points[n].value.value.s16));
            break;
        case OSENS_DT_U32:
            OS_UTIL_LOG(1, ("Point %02d: %d\n", n, sensor_points.points[n].value.value.u32));
            break;
        case OSENS_DT_S32:
            OS_UTIL_LOG(1, ("Point %02d: %d\n", n, sensor_points.points[n].value.value.s32));
            break;
        case OSENS_DT_U64:
            OS_UTIL_LOG(1, ("Point %02d: %d\n", n, sensor_points.points[n].value.value.u64));
            break;
        case OSENS_DT_S64:
            OS_UTIL_LOG(1, ("Point %02d: %d\n", n, sensor_points.points[n].value.value.s64));
            break;
        case OSENS_DT_FLOAT:
            OS_UTIL_LOG(1, ("Point %02d: %f\n", n, sensor_points.points[n].value.value.fp32));
            break;
        case OSENS_DT_DOUBLE:
            OS_UTIL_LOG(1, ("Point %02d: %f\n", n, sensor_points.points[n].value.value.fp64));
            break;
        default:
            break;
       }
    }
}
#endif

void* osens_mote_rx_serial(void *p)
{
    unsigned char data;
    while (1)
    {
        if (os_serial_read_byte(serial,&data))
        {
            if (num_rx_bytes < OSENS_MAX_FRAME_SIZE)
                frame[num_rx_bytes] = (uint8_t) data;

            num_rx_bytes++;

            if (num_rx_bytes >= OSENS_MAX_FRAME_SIZE)
                num_rx_bytes = 0;
        }
        else
        {
            os_kernel_sleep(5);
        }
    }
}

uint8_t osens_mote_init_v2(void)
{
    os_serial_options_t serial_options = { OS_SERIAL_BR_115200, OS_SERIAL_PR_NONE, OS_SERIAL_PB_1, 30 };

    serial = os_serial_open(serial_options);

    memset(&sm_state, 0, sizeof(osens_mote_sm_state_t));
    sm_state.state = OSENS_STATE_INIT;
    tick_counter = 0;

    sm_thread = os_kernel_create(osens_mote_tick, "SM_THREAD", (os_thread_arg) 0, os_kernel_get_def_pri(), os_kernel_get_def_stack(), os_kernel_get_def_time_slice(), 1);
    rx_thread = os_kernel_create(osens_mote_rx_serial, "RX_THREAD", (os_thread_arg) 0, os_kernel_get_def_pri(), os_kernel_get_def_stack(), os_kernel_get_def_time_slice(), 1);


    while (1)
    {
        os_kernel_sleep(100);
    };
    return 0;
}


static uint8_t osens_mote_pack_send_frame(osens_cmd_req_t *cmd, uint8_t cmd_size)
{
    uint8_t size;

    size = osens_pack_cmd_req(cmd, frame);

    if (size != cmd_size)
        return OSENS_STATE_EXEC_ERROR;

    if (osens_mote_send_frame(frame, cmd_size) != cmd_size)
        return OSENS_STATE_EXEC_ERROR;

    return OSENS_STATE_EXEC_OK;
}

static uint8_t osens_mote_sm_func_pt_val_ans(osens_mote_sm_state_t *st)
{
    uint8_t point;
    uint8_t size;
    uint8_t ans_size;

    point = acquisition_schedule.scan.index[st->point_index];
    ans_size = 6 + datatype_sizes[sensor_points.points[point].desc.type];

    size = osens_unpack_cmd_res(&ans, frame, ans_size);

    // retry ?
    if (size != ans_size || ans.hdr.addr != (OSENS_REGMAP_READ_POINT_DATA_1 + point))
        return OSENS_STATE_EXEC_OK;

    // ok, save and go to the next
    memcpy(&sensor_points.points[point].value, &ans.payload.point_value_cmd, sizeof(osens_point_t));

    st->retries = 0;
    st->point_index++;

#if TRACE_ON
    osens_mote_show_values();
#endif

    return OSENS_STATE_EXEC_OK;
}

static uint8_t osens_mote_sm_func_req_pt_val(osens_mote_sm_state_t *st)
{
    uint8_t point;

    // end of point reading
    if (st->point_index >= acquisition_schedule.scan.num_of_points)
        return OSENS_STATE_EXEC_WAIT_ABORT;

    // error condition after 3 retries
    st->retries++;
    if (st->retries > 3)
        return OSENS_STATE_EXEC_ERROR;

    point = acquisition_schedule.scan.index[st->point_index];
    cmd.hdr.addr = OSENS_REGMAP_READ_POINT_DATA_1 + point;
    st->trmout_counter = 0;
    st->trmout = MS2TICK(5000);
    return osens_mote_pack_send_frame(&cmd, 4);

}

static uint8_t osens_mote_sm_func_run_sch(osens_mote_sm_state_t *st)
{
    uint8_t n;

    acquisition_schedule.scan.num_of_points = 0;

    for (n = 0; n < acquisition_schedule.num_of_points; n++)
    {

        if (acquisition_schedule.points[n].counter > 0)
            acquisition_schedule.points[n].counter--;

        if (acquisition_schedule.points[n].counter == 0)
        {
            // n: point index in the schedule database
            // index: point index in the points database
            uint8_t index = acquisition_schedule.points[n].index;

            acquisition_schedule.scan.index[acquisition_schedule.scan.num_of_points] = index;
            acquisition_schedule.scan.num_of_points++;
            // restore counter value for next cycle
            acquisition_schedule.points[n].counter = acquisition_schedule.points[n].sampling_time_x250ms;
        }

    }

    if (acquisition_schedule.scan.num_of_points > 0)
    {
        st->point_index = 0;
        st->retries = 0;

#if TRACE_ON
    {
            OS_UTIL_LOG(1, ("\n"));
            OS_UTIL_LOG(1, ("Next Scan\n"));
            OS_UTIL_LOG(1, ("=========\n"));
            for (n = 0; n < acquisition_schedule.scan.num_of_points; n++)
            {
                OS_UTIL_LOG(1, ("--> %u [%u]\n", n,acquisition_schedule.scan.index[n]));
            }
    }
#endif

        return OSENS_STATE_EXEC_WAIT_ABORT;
    }
    else
        return OSENS_STATE_EXEC_WAIT_OK;
}

static uint8_t osens_mote_sm_func_build_sch(osens_mote_sm_state_t *st)
{
    uint8_t n, m;

    acquisition_schedule.num_of_points = 0;

    for (n = 0, m = 0; n < board_info.num_of_points; n++)
    {
        if ((sensor_points.points[n].desc.access_rights & OSENS_ACCESS_READ_ONLY) &&
            (sensor_points.points[n].desc.sampling_time_x250ms > 0))
        {
            acquisition_schedule.points[m].index = n;
            acquisition_schedule.points[m].counter = sensor_points.points[n].desc.sampling_time_x250ms;
            acquisition_schedule.points[m].sampling_time_x250ms = sensor_points.points[n].desc.sampling_time_x250ms;

            m++;
            acquisition_schedule.num_of_points++;
        }
    }

#if TRACE_ON
    OS_UTIL_LOG(1,("\n"));
    OS_UTIL_LOG(1, ("Schedule\n"));
    OS_UTIL_LOG(1, ("========\n"));
    for (n = 0; n < acquisition_schedule.num_of_points; n++)
    {
        OS_UTIL_LOG(1, ("[%d] point %02d at %dms\n", n, acquisition_schedule.points[n].index, acquisition_schedule.points[n].sampling_time_x250ms * 250));
    }
#endif

    return OSENS_STATE_EXEC_OK;
}

static uint8_t osens_mote_sm_func_pt_desc_ans(osens_mote_sm_state_t *st)
{
    uint8_t size;
    uint8_t ans_size = 20;

    size = osens_unpack_cmd_res(&ans, frame, ans_size);

    if (size != ans_size || (ans.hdr.addr != OSENS_REGMAP_POINT_DESC_1 + st->point_index))
        return OSENS_STATE_EXEC_ERROR;

    // save description and type, value is not available yet
    memcpy(&sensor_points.points[st->point_index].desc, &ans.payload.point_desc_cmd, sizeof(osens_point_desc_t));
    sensor_points.points[st->point_index].value.type = sensor_points.points[st->point_index].desc.type;

#if TRACE_ON
    {
        uint8_t n = st->point_index;
        OS_UTIL_LOG(1, ("\n"));
        OS_UTIL_LOG(1, ("Point %02d info\n", n));
        OS_UTIL_LOG(1, ("=============\n"));
        OS_UTIL_LOG(1, ("Name     : %-8s\n", sensor_points.points[n].desc.name));
        OS_UTIL_LOG(1, ("Type     : %d\n", sensor_points.points[n].desc.type));
        OS_UTIL_LOG(1, ("Unit     : %d\n", sensor_points.points[n].desc.unit));
        OS_UTIL_LOG(1, ("Rights   : %02X\n", sensor_points.points[n].desc.access_rights));
        OS_UTIL_LOG(1, ("Sampling : %d\n\n", sensor_points.points[n].desc.sampling_time_x250ms));
    }
#endif

    st->retries = 0;
    st->point_index++;
    sensor_points.num_of_points = st->point_index;

    return OSENS_STATE_EXEC_OK;
}

static uint8_t osens_mote_sm_func_req_pt_desc(osens_mote_sm_state_t *st)
{
    if (st->point_index >= board_info.num_of_points)
        return OSENS_STATE_EXEC_WAIT_ABORT;

    // error condition after 3 retries
    st->retries++;
    if (st->retries > 3)
        return OSENS_STATE_EXEC_ERROR;

    cmd.hdr.addr = OSENS_REGMAP_POINT_DESC_1 + st->point_index;
    st->trmout_counter = 0;
    st->trmout = MS2TICK(5000);
    return osens_mote_pack_send_frame(&cmd, 4);
}

static uint8_t osens_mote_sm_func_proc_brd_id_ans(osens_mote_sm_state_t *st)
{
    uint8_t size;
    uint8_t ans_size = 28;

    st->point_index = 0;

    size = osens_unpack_cmd_res(&ans, frame, ans_size);

    if (size != ans_size)
        return OSENS_STATE_EXEC_ERROR;

    memcpy(&board_info, &ans.payload.brd_id_cmd, sizeof(osens_brd_id_t));

    if ((board_info.num_of_points == 0) || (board_info.num_of_points > OSENS_MAX_POINTS))
        return OSENS_STATE_EXEC_ERROR;

#if TRACE_ON
    OS_UTIL_LOG(1, ("\n"));
    OS_UTIL_LOG(1, ("Board info\n"));
    OS_UTIL_LOG(1, ("==========\n"));
    OS_UTIL_LOG(1, ("Manufactor : %-8s\n", board_info.manufactor));
    OS_UTIL_LOG(1, ("Model      : %-8s\n", board_info.model));
    OS_UTIL_LOG(1, ("ID         : %08X\n", board_info.sensor_id));
    OS_UTIL_LOG(1, ("HW REV     : %02X\n", board_info.hardware_revision));
    OS_UTIL_LOG(1, ("Capabilties: %02X\n", board_info.cabalities));
    OS_UTIL_LOG(1, ("Points     : %d\n\n", board_info.num_of_points));
#endif

    sensor_points.num_of_points = 0;
    st->retries = 0;

    return OSENS_STATE_EXEC_OK;
}

static uint8_t osens_mote_sm_func_req_brd_id(osens_mote_sm_state_t *st)
{
    cmd.hdr.size = 4;
    cmd.hdr.addr = OSENS_REGMAP_BRD_ID;
    st->trmout_counter = 0;
    st->trmout = MS2TICK(5000);
    return osens_mote_pack_send_frame(&cmd, 4);
}


static uint8_t osens_mote_sm_func_proc_itf_ver_ans(osens_mote_sm_state_t *st)
{
    uint8_t size;
    uint8_t ans_size = 6;

    size = osens_unpack_cmd_res(&ans, frame, ans_size);

    if (size != ans_size)
        return OSENS_STATE_EXEC_ERROR;

    if ((OSENS_ANS_OK != ans.hdr.status) || (OSENS_LATEST_VERSION != ans.payload.itf_version_cmd.version))
        return OSENS_STATE_EXEC_ERROR;

    return OSENS_STATE_EXEC_OK;
}


static uint8_t osens_mote_sm_func_wait_ans(osens_mote_sm_state_t *st)
{

    /* TODO!!!! MELHORAR O Tratamento do final de frame
    * Devido ao problema da UART nao estar reconhecendo o final do frame
    * a rotina esperar um ciclo de scan (250ms) entao se recebeu algum dado ele diz
    * que recebeu o frame. Posteriormente sera tratado o frame e verificado se recebeu tudo
    */
    if ((st->trmout_counter > 0) && (num_rx_bytes > 3))
    {
        st->frame_arrived = 1;
        num_rx_bytes = 0;
    }

    if (st->frame_arrived)
    {
        st->frame_arrived = 0;
        return OSENS_STATE_EXEC_WAIT_STOP;
    }

    st->trmout_counter++;

    if (st->trmout_counter > st->trmout)
    {
        //se ocorreu timeout e existe bytes no buffer rx considero que recebeu msg
        if (num_rx_bytes > 0)
        {
            return OSENS_STATE_EXEC_WAIT_STOP;
        }
        else
        {
            return OSENS_STATE_EXEC_WAIT_ABORT;
        }
    }

    return OSENS_STATE_EXEC_WAIT_OK;
}


static uint8_t osens_mote_sm_func_req_ver(osens_mote_sm_state_t *st)
{
    cmd.hdr.addr = OSENS_REGMAP_ITF_VERSION;
    st->trmout_counter = 0;
    st->trmout = MS2TICK(5000);
    return osens_mote_pack_send_frame(&cmd, 4);
}


static uint8_t osens_mote_sm_func_init(osens_mote_sm_state_t *st)
{
    uint8_t ret = OSENS_STATE_EXEC_OK;

    memset(&cmd, 0, sizeof(cmd));
    memset(&ans, 0, sizeof(ans));
    memset(&sensor_points, 0, sizeof(sensor_points));
    memset(&board_info, 0, sizeof(board_info));
    memset(&acquisition_schedule, 0, sizeof(acquisition_schedule));
    memset(st, 0, sizeof(osens_mote_sm_state_t));

    num_rx_bytes = 0;

    return ret;
}

void osens_mote_sm(void)
{
    uint8_t ret;

#ifdef TRACE_ON
    uint8_t ls = sm_state.state;
#endif

    ret = osens_mote_sm_table[sm_state.state].func(&sm_state);

    /*
    if (flagErrorOccurred)
    {
        //reset uart
        uart1_clearTxInterrupts();
        uart1_clearRxInterrupts();      // clear possible pending interrupts
        uart1_enableInterrupts();       // Enable USCI_A1 TX & RX interrupt

        flagErrorOccurred = 0;
    }
    */

    switch (ret)
    {
    case OSENS_STATE_EXEC_OK:
    case OSENS_STATE_EXEC_WAIT_STOP:
        sm_state.state = osens_mote_sm_table[sm_state.state].next_state;
        break;
    case OSENS_STATE_EXEC_WAIT_OK:
        // still waiting
        break;
    case OSENS_STATE_EXEC_WAIT_ABORT:
        // wait timeout
        sm_state.state = osens_mote_sm_table[sm_state.state].abort_state;
        break;
    case OSENS_STATE_EXEC_ERROR:
    default:
        sm_state.state = osens_mote_sm_table[sm_state.state].error_state;
        break;
    }

#ifdef TRACE_ON
    printf("[SM]  %llu    (%02d) %-16s -> (%02d) %-16s\n", tick_counter, ls, sm_states_str[ls], sm_state.state,sm_states_str[sm_state.state]);
#endif

}

const osens_mote_sm_table_t osens_mote_sm_table[] =
{     //{ func,                                   next_state,                      abort_state,                error_state         }
    { osens_mote_sm_func_init, OSENS_STATE_SEND_ITF_VER, OSENS_STATE_INIT, OSENS_STATE_INIT }, // OSENS_STATE_INIT
    { osens_mote_sm_func_req_ver, OSENS_STATE_WAIT_ITF_VER_ANS, OSENS_STATE_INIT, OSENS_STATE_INIT }, // OSENS_STATE_SEND_ITF_VER
    { osens_mote_sm_func_wait_ans, OSENS_STATE_PROC_ITF_VER, OSENS_STATE_INIT, OSENS_STATE_INIT }, // OSENS_STATE_WAIT_ITF_VER_ANS
    { osens_mote_sm_func_proc_itf_ver_ans, OSENS_STATE_SEND_BRD_ID, OSENS_STATE_INIT, OSENS_STATE_INIT }, // OSENS_STATE_PROC_ITF_VER
    { osens_mote_sm_func_req_brd_id, OSENS_STATE_WAIT_BRD_ID_ANS, OSENS_STATE_INIT, OSENS_STATE_INIT }, // OSENS_STATE_SEND_BRD_ID
    { osens_mote_sm_func_wait_ans, OSENS_STATE_PROC_BRD_ID, OSENS_STATE_INIT, OSENS_STATE_INIT }, // OSENS_STATE_WAIT_BRD_ID_ANS
    { osens_mote_sm_func_proc_brd_id_ans, OSENS_STATE_SEND_PT_DESC, OSENS_STATE_INIT, OSENS_STATE_INIT }, // OSENS_STATE_PROC_BRD_ID
    { osens_mote_sm_func_req_pt_desc, OSENS_STATE_WAIT_PT_DESC_ANS, OSENS_STATE_BUILD_SCH, OSENS_STATE_INIT }, // OSENS_STATE_SEND_PT_DESC
    { osens_mote_sm_func_wait_ans, OSENS_STATE_PROC_PT_DESC, OSENS_STATE_SEND_PT_DESC, OSENS_STATE_INIT }, // OSENS_STATE_WAIT_PT_DESC_ANS
    { osens_mote_sm_func_pt_desc_ans, OSENS_STATE_SEND_PT_DESC, OSENS_STATE_INIT, OSENS_STATE_INIT }, // OSENS_STATE_PROC_PT_DESC
    { osens_mote_sm_func_build_sch, OSENS_STATE_RUN_SCH, OSENS_STATE_INIT, OSENS_STATE_INIT }, // OSENS_STATE_BUILD_SCH
    { osens_mote_sm_func_run_sch, OSENS_STATE_RUN_SCH, OSENS_STATE_SEND_PT_VAL, OSENS_STATE_INIT }, // OSENS_STATE_RUN_SCH
    { osens_mote_sm_func_req_pt_val, OSENS_STATE_WAIT_PT_VAL_ANS, OSENS_STATE_RUN_SCH, OSENS_STATE_INIT }, // OSENS_STATE_SEND_PT_VAL
    { osens_mote_sm_func_wait_ans, OSENS_STATE_PROC_PT_VAL, OSENS_STATE_SEND_PT_VAL, OSENS_STATE_INIT }, // OSENS_STATE_WAIT_PT_VAL_ANS
    { osens_mote_sm_func_pt_val_ans, OSENS_STATE_SEND_PT_VAL, OSENS_STATE_INIT, OSENS_STATE_INIT }, // OSENS_STATE_PROC_PT_VAL
};

uint8_t osens_init(void)
{
    osens_mote_init_v2();
    return 0;
}
