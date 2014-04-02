#include <string.h>
#include <stdint.h>
#include "sens_itf.h"
#include "../os/os_defs.h"
#include "../os/os_timer.h"
#include "sens_util.h"
#include "../util/buf_io.h"
#include "../util/crc16.h"

uint8_t sens_itf_unpack_point_value(sens_itf_cmd_point_t *point, uint8_t *buf)
{
    uint8_t size = 0;

    switch (point->type)
    {
    case SENS_ITF_DT_U8:
        point->value.u8 = buf_io_get8_fl(buf);
        size = 1;
        break;
    case SENS_ITF_DT_S8:
        point->value.s8 = buf_io_get8_fl(buf);
        size = 1;
        break;
    case SENS_ITF_DT_U16:
        point->value.u16 = buf_io_get16_fl(buf);
        size = 2;
        break;
    case SENS_ITF_DT_S16:
        point->value.s16 = buf_io_get16_fl(buf);
        size = 2;
        break;
    case SENS_ITF_DT_U32:
        point->value.u32 = buf_io_get32_fl(buf);
        size = 4;
        break;
    case SENS_ITF_DT_S32:
        point->value.s32 = buf_io_get32_fl(buf);
        size = 4;
        break;
    case SENS_ITF_DT_U64:
        point->value.u64 = buf_io_get64_fl(buf);
        size = 8;
        break;
    case SENS_ITF_DT_S64:
        point->value.s64 = buf_io_get64_fl(buf);
        size = 8;
        break;
    case SENS_ITF_DT_FLOAT:
        point->value.fp32 = buf_io_getf_fl(buf);
        size = 4;
        break;
    case SENS_ITF_DT_DOUBLE:
        point->value.fp64 = buf_io_getd_fl(buf);
        size = 8;
        break;
    default:
        break;
    }

    return size;
}

uint8_t sens_itf_pack_point_value(const sens_itf_cmd_point_t *point, uint8_t *buf)
{
    uint8_t size = 0;

    switch (point->type)
    {
    case SENS_ITF_DT_U8:
        buf_io_put8_tl(point->value.u8, buf);
        size = 1;
        break;
    case SENS_ITF_DT_S8:
        buf_io_put8_tl(point->value.s8, buf);
        size = 1;
        break;
    case SENS_ITF_DT_U16:
        buf_io_put16_tl(point->value.u16, buf);
        size = 2;
        break;
    case SENS_ITF_DT_S16:
        buf_io_put16_tl(point->value.s16, buf);
        size = 2;
        break;
    case SENS_ITF_DT_U32:
        buf_io_put32_tl(point->value.u32, buf);
        size = 4;
        break;
    case SENS_ITF_DT_S32:
        buf_io_put32_tl(point->value.s32, buf);
        size = 4;
        break;
    case SENS_ITF_DT_U64:
        buf_io_put64_tl(point->value.u64, buf);
        size = 8;
        break;
    case SENS_ITF_DT_S64:
        buf_io_put64_tl(point->value.s64, buf);
        size = 8;
        break;
    case SENS_ITF_DT_FLOAT:
        buf_io_putf_tl(point->value.fp32, buf);
        size = 4;
        break;
    case SENS_ITF_DT_DOUBLE:
        buf_io_putd_tl(point->value.fp64, buf);
        size = 8;
        break;
    default:
        break;
    }

    return size;
}
