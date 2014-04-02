#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "sens_util.h"

#define MAX_LOG_BUF 256
static char buffer[MAX_LOG_BUF];

static volatile int sens_util_log_started = 0;

void sens_util_assert(int cond)
{
    assert(cond);
}

const uint8_t *sens_util_strip_path(const uint8_t *filename)
{
	int pos;

	pos = strlen(filename) - 1; // avoiding null terminator

	while( (filename[pos] != '\\') && (pos > 0) ) 
		pos--;

	if(pos != 0) 
		pos++; // removing "\"
	
	return &filename[pos];
}

void sens_util_log(int cond, const uint8_t *line, ...)
{
	va_list argp;
   
    if((!cond) || (!sens_util_log_started))
		return;

	va_start(argp, line);
    vsnprintf(buffer,MAX_LOG_BUF,line, argp);
    va_end(argp);
    
    buffer[MAX_LOG_BUF-1] = '\0';

	// TODO
	// define destination for this buffer: ethernet, serial, console, syslog ...
	// 
    printf("%s",buffer);
}

void sens_util_dump_frame(const uint8_t * const data, int len)
{
	int i, j, k;
	uint8_t buf[50];

	for(k = 0 ; k < len ; k += 16)
	{
		for(i = k, j = 0 ; ( i< (k+16) ) && ( i < len ) ; i++, j+=3 )
			sprintf((char *)&buf[j],"%02X ",data[i]);
		buf[j] = '\0';
		sens_util_log(1,"%s",buf);
	}
}

int sens_util_log_stop(void)
{
	if(sens_util_log_started)
        sens_util_log_started  = 0;

	return 0;
}

int sens_util_log_start(void)
{
	if(!sens_util_log_started)
        sens_util_log_started  = 1;

	return 0;
}
