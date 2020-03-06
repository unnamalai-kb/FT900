/*
 * iot_i2c.c

 *
 *  Created on: 16 Jan 2020
 *      Author: prabhakaran.d
 */
#include <ft900.h>
#include <iot_uart.h>
#include <debug.h>
#define I2C_MUX_ADD         0x70

static uint8_t current_slot = 0xff;

#if UNUSED_DEMO_FUNCTIONS
void iot_i2c_init(){

	/* RESET Pin of I2C 8-Channel chip. "0" to reset system master */
    gpio_function(64,pad_gpio64);
    gpio_dir(64,pad_dir_output);
    gpio_write(64,0);

    sys_enable(sys_device_i2c_master);
    gpio_function(44, pad_i2c0_scl);
    gpio_pull(44, pad_pull_none);
    gpio_function(45, pad_i2c0_sda);
    gpio_pull(45, pad_pull_none);
    i2cm_init(I2CM_NORMAL_SPEED, 100000);
}


void iot_i2c_exit(uint8_t channel){

}
#endif

void iot_i2c_slot(uint8_t slot_number){
    uint8_t sn = (1 << (slot_number + 2));
    if(current_slot != sn){
        current_slot = sn;
    }else{
        return;
    }
    gpio_write(64,0);
    usleep(50);
    gpio_write(64,1);
    usleep(50);
    i2cm_write(I2C_MUX_ADD << 1,0,&sn,1);
    DPRINTF_INFO("Slot selected");
}

#if UNUSED_DEMO_FUNCTIONS
void iot_digi2_write(uint8_t sn,uint8_t address,uint8_t format,uint8_t brightness,uint8_t val){

    DPRINTF_INFO("S: %d A: 0x%x F: %d B: %d V: %d",sn,address,format, brightness,val);
    iot_i2c_slot(sn);
    i2cm_write(address << 1,0x04,&brightness,1);
    if(format == 0){
        val = (val >= 0xff) ? 0xff : val;
        i2cm_write(address << 1,0x01,&val,1);
    }else if(format == 1){
        val = (val > 99) ? 99 : val;
        i2cm_write(address << 1,0x02,&val,1);
    }else
    {
        uint8_t data[2];
        uint8_t raw[] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
        val = (val > 99) ? 99 : val;
        data[0] = raw[val / 10] | 0x80;
        data[1] = raw[val % 10];
        i2cm_write(address << 1,0x00,data,2);
    }
}
#endif

void iot_pot_read(uint8_t sn,uint8_t address,uint8_t scale,uint8_t *val){
    DPRINTF_INFO("S: %d A: 0x%x F: %d",sn,address,scale);
    iot_i2c_slot(sn);
    i2cm_read(address << 1,scale,val,1);
}


void iot_led_write(uint8_t sn,uint8_t address,uint8_t fadeouttime,uint8_t *val){

    uint8_t data[4];
    data[0] = val[0];
    data[1] = val[1];
    data[2] = val[2];
    data[3] = fadeouttime;
    DPRINTF_INFO("S: %d A: 0x%x F: %d",sn,address,fadeouttime);
    DPRINTF_INFO("%d %d %d",data[0],data[1],data[2]);
    iot_i2c_slot(sn);
    i2cm_write(address << 1,0x01,data,sizeof(data));
}

