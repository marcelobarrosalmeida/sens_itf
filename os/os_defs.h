#ifndef __OS_DEFS__
#define __OS_DEFS__ 

#ifdef __cplusplus
extern "C" {
#endif

enum os_return_codes_e
{
    OS_TIMEOUT = -1,
    OS_ERROR = 0,
    OS_SUCCESS = 1,
};

/** Infinite timeout */
#define OS_INFINTE_TMROUT       0xFFFFFFFFL

#ifdef __cplusplus
}
#endif

#endif /*  __OS_DEFS__ */

