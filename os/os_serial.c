#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include "windows.h"

#include "os_util.h"
#include "os_serial.h"


#define OS_DBG_SER_DRV 0

#define MAX_PORT_NAME 10

struct os_serial_drv_s 
{
    int          port;
    int          bps;
    HANDLE       hcom;
    DCB          dcb;
    COMMTIMEOUTS to;
};

os_serial_t os_serial_open(os_serial_options_t options)
{
    int is_error = 1;    
    char port[10];

    os_serial_t ser = (os_serial_t) calloc(1,sizeof(struct os_serial_drv_s));
    OS_UTIL_ASSERT(ser);

    ser->port = options.port;
    ser->bps  = options.bps;

    sprintf_s(port, 10,  "\\\\.\\COM%d", ser->port);

	OS_UTIL_LOG( OS_DBG_SER_DRV, ("Port %s, bps=%d)\n",port,options.bps) );
    
    ser->hcom = CreateFile( (LPCTSTR) port, 
		GENERIC_READ | GENERIC_WRITE, 
		0, 
		NULL, 
		OPEN_EXISTING, 
		0, 
		NULL );

    if (ser->hcom == INVALID_HANDLE_VALUE) 
	{
		OS_UTIL_LOG( OS_DBG_SER_DRV, ("CreateFile error: %d\n", GetLastError() ) );
		return 0;
	}

    if (!GetCommState(ser->hcom, &(ser->dcb))) 
	{
        if (INVALID_HANDLE_VALUE != ser->hcom)
            CloseHandle(ser->hcom);
		OS_UTIL_LOG( OS_DBG_SER_DRV, ("GetCommState error: %d\n", GetLastError()) );
		return 0;
    }
    
    // set bps, 8 data bits, no parity, no flow control, and 1 stop bit.
    switch (ser->bps) 
	{
        case(OS_SERIAL_BR_9600):
            ser->dcb.BaudRate = CBR_9600;
            break;
        case(OS_SERIAL_BR_19200):
            ser->dcb.BaudRate = CBR_19200;
            break;            
        case(OS_SERIAL_BR_115200):
            ser->dcb.BaudRate = CBR_115200;
            break;
        default:
            CloseHandle(ser->hcom);
			OS_UTIL_LOG( OS_DBG_SER_DRV, ("Invalid bit rate error: %d\n", GetLastError()) );
			return 0;
            break;
    }

    ser->dcb.ByteSize      = 8;            // data size, xmit, and rcv
    ser->dcb.Parity        = NOPARITY;     // no parity bit
    ser->dcb.StopBits      = ONESTOPBIT;   // one stop bit
    ser->dcb.fAbortOnError = TRUE;         
    ser->dcb.fOutxCtsFlow  = FALSE;        // send only if CTS is high
    ser->dcb.fOutX         = FALSE;
    ser->dcb.fInX          = FALSE;
    // ser->dcb.fNull       = TRUE;

    if (!SetCommState(ser->hcom, &(ser->dcb))) 
	{
		CloseHandle(ser->hcom);
		OS_UTIL_LOG( OS_DBG_SER_DRV, ("SetCommState error: %d\n", GetLastError() ) );
		return 0;
    }

    /* Set timeout values */
    memset(&(ser->to), 0, sizeof(ser->to));
    ser->to.ReadIntervalTimeout         = 1;    /* 1 msec timeout per byte */
    ser->to.ReadTotalTimeoutConstant    = 0;    /* No extra timeout for read operation */
    ser->to.ReadTotalTimeoutMultiplier  = 1;    /* Use byte's timeout */
    ser->to.WriteTotalTimeoutConstant   = 0;    /* Write data immediately */
    ser->to.WriteTotalTimeoutMultiplier = 0;

    if (!SetCommTimeouts(ser->hcom, &(ser->to))) 
	{
		CloseHandle(ser->hcom);
		OS_UTIL_LOG( OS_DBG_SER_DRV, ("SetCommTimeouts error: %d\n", GetLastError()) );
		return 0;
    }

	return ser;
}

int os_serial_read(os_serial_t ser, unsigned char *data, int len)
{
    DWORD dwRead = 0;

	OS_UTIL_ASSERT(ser);
	OS_UTIL_ASSERT(data);

    if (ReadFile((HANDLE)ser->hcom, data, len, &dwRead, NULL) ) 
	{
		OS_UTIL_LOG( OS_DBG_SER_DRV, ("Read %d of %d Bytes\n", dwRead,len) );
		/*dump_frame(data,dwRead);*/
		return dwRead;
	}

	OS_UTIL_LOG( OS_DBG_SER_DRV, ("Error reading byte: %d. os_serial_win32::os_serial_read.\n", GetLastError()) );
	return -1;
}

int os_serial_write(os_serial_t ser, unsigned char *data, int len)
{
    DWORD dwWritten = 0;

	OS_UTIL_ASSERT(ser);
	OS_UTIL_ASSERT(data);
	OS_UTIL_ASSERT(len >= 0);

	if(len == 0) return 0;

    if( WriteFile(ser->hcom, data, len, &dwWritten, NULL) == TRUE )
	{
		OS_UTIL_LOG( OS_DBG_SER_DRV, ("Writen %d of %d Bytes\n", dwWritten,len) );
		/*dump_frame(data,dwWritten);*/
		return dwWritten;
	}

	OS_UTIL_LOG( OS_DBG_SER_DRV, ("Write error: %d (len=%d)\n", GetLastError(),len) );
	return -1;
}

int os_serial_read_byte(os_serial_t ser, unsigned char *data)
{
    DWORD dwRead = 0;

	OS_UTIL_ASSERT(ser);
	OS_UTIL_ASSERT(data);

    if ( ReadFile((HANDLE)ser->hcom, data, sizeof(unsigned char), &dwRead, NULL) ) 
	{
        if (dwRead == sizeof(unsigned char)) 
		{
			OS_UTIL_LOG( OS_DBG_SER_DRV, ("Serial IN: %02X\n", *data ) );
      return 1;
		}
	}

	OS_UTIL_LOG( OS_DBG_SER_DRV, ("Error reading byte: %d. os_serial_win32::os_serial_read_byte.\n", GetLastError()) );
	return 0;
}

int os_serial_write_byte(os_serial_t ser, unsigned char data)
{
    DWORD dwWritten = 0;

	OS_UTIL_ASSERT(ser);

  if( WriteFile(ser->hcom, (LPCVOID) &data, sizeof(unsigned char), &dwWritten, NULL) == TRUE )
	{
		if(dwWritten == sizeof(unsigned char))
		{
			OS_UTIL_LOG( OS_DBG_SER_DRV, ("Serial OUT: %02X\n", data ) );
			return 1;
		}
	}

	OS_UTIL_LOG( OS_DBG_SER_DRV, ("Write error: %d \n", GetLastError()) );
	return 0;
}

int os_serial_close(os_serial_t ser)
{
	OS_UTIL_ASSERT(ser);

	OS_UTIL_LOG( OS_DBG_SER_DRV, ("Closing port %d\n", ser->port) );

    if (ser->hcom != INVALID_HANDLE_VALUE)
        CloseHandle(ser->hcom);

    free(ser);

	return 0;
}


