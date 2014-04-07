/**
    @file os_util.h
    @brief Utilities routines
*/
#ifndef __OS_UTIL__
#define __OS_UTIL__ 

#ifdef __cplusplus
extern "C" {
#endif

/**
    @defgroup OSUTIL Utilities
    @ingroup OSGLOBALS
    @{
*/

/** 
    Assertion like function.
    
    @param cond Assertion condition.
 */
extern void os_util_assert(int cond);

/**
    Message log utility.
    
    @param line Variable parameters list like printf.
*/
extern void os_util_log(const unsigned char *line, ...);

/**
    Start log.

    @retval 1 Log not started.
    @retval 0 Log started.
*/
extern int os_util_log_start(void);

/**
	Stop log .

@retval 1 Log not stopped.
@retval 0 Log stopped.
*/
extern int os_util_log_stop(void);

/**
    Remove path and return only the file name.
    
    @param filename file with full path name
    @return filename without path
*/
extern const uint8_t *os_util_strip_path(const unsigned char *filename);

/**
    Print a buffer in hexadecimal (16 byte per row).

    @param data Data to be printed.
    @param len Data length.
*/
extern void os_util_dump_frame(const unsigned char *const data, int len);

/**
    @def OS_UTIL_ASSERT
    @brief Assertion macro
*/
#define OS_UTIL_ASSERT(cond)   os_util_assert( !!(cond) )
#define OS_UTIL_LOG(cond,expr)  \
    do {\
        if( (cond) ){ \
            os_util_log expr; \
        } \
    } while(0)

/**@}*/

#ifdef __cplusplus
}
#endif

#endif /*  __OS_UTIL__ */

