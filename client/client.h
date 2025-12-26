#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

enum COMMON_PACKET_TYPE{
	LIGHT_PACKET = 0,
	CAMERA_XY_PACKET,
	CHASSIS_PACKET,
	BATTERY_PACKET,
	SERVER_ANSWER,
};

enum SERVER_ANSWER
{
	REQUEST_OK = 0,
	REQUEST_WRONG_CRC,
	REQUEST_UNKNOW_PCKT_ID,
};

typedef struct{
	uint32_t packet_id;
	uint32_t packet_number;
	uint16_t data_size;
	uint16_t crc;
	uint8_t ptr_data[];
}COMMON_PACKET_T;

typedef struct{
	uint8_t cam_id;
	uint8_t cam_val;
}LIGHT_DATA_T;

typedef struct{
	int pwm_pol;
	uint8_t  chassis1_dir;
	uint8_t  chassis2_dir;
}CHASSIS_DATA_T;

typedef struct{
	int  x_pwm;
	int  y_pwm;
}CAMERA_XY_DATA_T;

typedef struct{
	uint16_t  status;
	uint16_t  raw_data;
}BATTERY_DATA_T;

#endif