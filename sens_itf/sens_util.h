/**
    @file sens_util.h
    @brief Utilities routines
*/
#ifndef __SENS_UTIL__
#define __SENS_UTIL__ 

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
extern void sens_util_assert(int cond);

/**
    Message log utility.
    
    @param cond Logs only when the condition is true.
    @param line Variable parameters list like printf.
*/
extern void sens_util_log(int cond, const uint8_t *line, ...);

/**
    Start log.

    @retval 1 Log not started.
    @retval 0 Log started.
*/
extern int sens_util_log_start(void);

/**
	Stop log .

@retval 1 Log not stopped.
@retval 0 Log stopped.
*/
extern int sens_util_log_stop(void);

/**
    Remove path and return only the file name.
    
    @param filename file with full path name
    @return filename without path
*/
extern const uint8_t *sens_util_strip_path(const uint8_t *filename);

/**
    Print a buffer in hexadecimal (16 byte per row).

    @param data Data to be printed.
    @param len Data length.
*/
extern void sens_util_dump_frame(const uint8_t *const data, int len);

/**
    @def SENS_UTIL_ASSERT
    @brief Assertion macro
*/
#define SENS_UTIL_ASSERT(cond) sens_util_assert( !!(cond) )

/**@}*/

#ifdef __cplusplus
}
#endif

#endif /*  __SENS_UTIL__ */

