#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <time.h> 

#include "os_util.h"

#define MAX_LOG_BUF 256
static char buffer[MAX_LOG_BUF];

static volatile int os_util_log_started = 0;

void os_util_assert(int cond)
{
    assert(cond);
}

const uint8_t *os_util_strip_path(const unsigned char *filename)
{
	int pos;

	pos = strlen(filename) - 1; // avoiding null terminator

	while( (filename[pos] != '\\') && (pos > 0) ) 
		pos--;

	if(pos != 0) 
		pos++; // removing "\"
	
	return &filename[pos];
}

void os_util_log(const unsigned char *line, ...)
{
    time_t cur_time;
	struct tm * timeinfo;
    
    va_list argp;
   
    if(!os_util_log_started)
		return;

    time(&cur_time);
    timeinfo = localtime (&cur_time);
    strftime(buffer,80,"[%x %X] ",timeinfo);
    printf("%s",buffer);

	va_start(argp, line);
    vsnprintf(buffer,MAX_LOG_BUF,line, argp);
    va_end(argp);
    
    buffer[MAX_LOG_BUF-1] = '\0';

    printf("%s",buffer);
}

void os_util_dump_frame(const unsigned char * const data, int len)
{
	int i, j, k;
	uint8_t buf[50];

	for(k = 0 ; k < len ; k += 16)
	{
		for(i = k, j = 0 ; ( i< (k+16) ) && ( i < len ) ; i++, j+=3 )
			sprintf((char *)&buf[j],"%02X ",data[i]);
		buf[j] = '\0';
		os_util_log("%s\n",buf);
	}
}

int os_util_log_stop(void)
{
	if(os_util_log_started)
        os_util_log_started  = 0;

	return 0;
}

int os_util_log_start(void)
{
	if(!os_util_log_started)
        os_util_log_started  = 1;

	return 0;
}
