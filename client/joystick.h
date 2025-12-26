#ifndef _JOYTICK_H_
#define _JOYTICK_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <stdatomic.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include <time.h>
#include <stdint.h>

enum CHASSIS_DIRECTION
{
	FORWARD = 0,
	BACK,
};

enum CHASSIS_TURN
{
	DIRECT = 0,
	RIGHT,
	LEFT
};

typedef struct{
	int axis;
	uint16_t axis_data;
}JOYSTICK_AXIS_DATA_T;


int  JoystickInit(char *path);
void JoystickClose();
void JoystickPollDispatcher();
void ChassisDispatcher();

#endif