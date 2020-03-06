/*
 * iot_i2c.h
 *
 *  Created on: 4 Feb 2020
 *      Author: prabhakaran.d
 */

#ifndef SOURCES_IOT_I2C_H_
#define SOURCES_IOT_I2C_H_

void iot_i2c_init();
void iot_i2c_exit();
void iot_digi2_write(uint8_t sn,uint8_t address,uint8_t format,uint8_t brightness,uint8_t val);
void iot_pot_read(uint8_t sn,uint8_t address,uint8_t scale,uint8_t *val);
void iot_led_write(uint8_t sn,uint8_t address,uint8_t fadeouttime,uint8_t *val);

#endif /* SOURCES_IOT_I2C_H_ */
