/*
 * json.h
 *
 *  Created on: Dec 12, 2019
 *      Author: richmond
 */

#ifndef _IOT_JSON_H_
#define _IOT_JSON_H_

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

uint32_t json_parse_int( const char* ptr, char* key );
char* json_parse_str( const char* ptr, char* key, int* len );
char* json_parse_str_ex( char* ptr, char* key, char end );
char* json_parse_object( const char* ptr, const char* key, int* len );
char* json_parse_object_array( const char* ptr, const char* key, char * len, uint idx);
char* json_parse_object_multiarray( const char* ptr, const char* key, int* len, int arr_idx, int obj_idx);

#endif /* _IOT_JSON_H_ */
