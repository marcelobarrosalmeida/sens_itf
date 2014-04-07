#ifndef __OS_SERIAL__
#define __OS_SERIAL__ 

#ifdef __cplusplus
extern "C" {
#endif

enum os_serial_baud_rate_e
{
	OS_SERIAL_BR_9600   = 9600,
    OS_SERIAL_BR_19200  = 19200,
	OS_SERIAL_BR_115200 = 115200
};

enum os_serial_parity_e
{
	OS_SERIAL_PR_NONE = 0,
	OS_SERIAL_PR_ODD,
	OS_SERIAL_PR_EVEN
};

enum os_serial_stop_bit_e
{
	OS_SERIAL_PB_1 = 1,
	OS_SERIAL_PB_2 = 2
};


enum os_serial_port_e
{
	OS_SERIAL_PORT_1 = 1,
	OS_SERIAL_PORT_2 = 2,
	OS_SERIAL_PORT_3 = 3,
	OS_SERIAL_PORT_4 = 4
};

typedef struct
{
	int bps;
	int parity;
	int stop_bits;
	int port;
} os_serial_options_t;

typedef struct os_serial_drv_s * os_serial_t;

extern os_serial_t os_serial_open(os_serial_options_t options);
extern int os_serial_read(os_serial_t ser, unsigned char *data, int len);
extern int os_serial_write(os_serial_t ser, unsigned char *data, int len);
extern int os_serial_read_byte(os_serial_t ser, unsigned char *data);
extern int os_serial_write_byte(os_serial_t ser, unsigned char data);
extern int os_serial_close(os_serial_t ser);


#ifdef __cplusplus
}
#endif

#endif /*  __OS_SERIAL__ */

