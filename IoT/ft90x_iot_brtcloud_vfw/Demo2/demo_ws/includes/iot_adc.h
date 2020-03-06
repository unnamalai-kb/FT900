/*
 * iot_adc.h
 *
 *  Created on: 3 Feb 2020
 *      Author: prabhakaran.d
 */

#ifndef INCLUDES_IOT_ADC_H_
#define INCLUDES_IOT_ADC_H_



typedef enum{
    adc_in_5_5,
    adc_in_10_10,
    adc_in_0_10
}ADCIn;

void iot_adc_init(ADCIn adc_in_type);
void iot_adc_run(uint16_t *raw,uint8_t max_channels);
void iot_adc_exit();

#endif /* INCLUDES_IOT_ADC_H_ */
