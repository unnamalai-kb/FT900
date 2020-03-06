#ifndef DS18B20_h
#define DS18B20_h

#include <ft900.h>

#define HIGH 1
#define LOW 0

#define ow_ds18b20_oe_pin 40
#define ow_ds18b20_data_pin 41

#define ow_ds18b20_oe_output()		gpio_dir(ow_ds18b20_oe_pin, pad_dir_output);gpio_pull(ow_ds18b20_oe_pin, pad_pull_none)
#define ow_ds18b20_oe_set()         gpio_write(ow_ds18b20_oe_pin, HIGH)
#define ow_ds18b20_oe_clr()         gpio_write(ow_ds18b20_oe_pin, LOW)

#define ow_ds18b20_dl_output()      gpio_dir(ow_ds18b20_data_pin, pad_dir_output);gpio_pull(ow_ds18b20_data_pin, pad_pull_none)
#define ow_ds18b20_dl_set()			gpio_write(ow_ds18b20_data_pin, HIGH)
#define ow_ds18b20_dl_clr()			gpio_write(ow_ds18b20_data_pin, LOW)

#define ow_ds18b20_dl_input()		gpio_dir(ow_ds18b20_data_pin, pad_dir_input);gpio_pull(ow_ds18b20_data_pin, pad_pull_pullup)
#define ow_ds18b20_dl_read()        gpio_read(ow_ds18b20_data_pin)

void ds18b20_send(char bit);
unsigned char ds18b20_read(void);
void ds18b20_send_byte(char data);
unsigned char ds18b20_read_byte(void);
unsigned char ds18b20_rst_pulse(void);
int iot_ow_read_temperature(uint16_t *t);
void iot_ow_init(void);
void iot_ow_exit(void);

#endif
