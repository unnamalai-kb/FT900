/*
 * pub_msg.c
 *
 *  Created on: 21 Jan 2020
 *      Author: srikanth.anandapu
 */

#include <stdint.h>
#include "iot_modem.h"

void pub_msg(char *s,char *fmt){
	char payload[600];
	strcpy(payload,s);
	tfp_sprintf(s, fmt, payload );
}



