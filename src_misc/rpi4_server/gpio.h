#ifndef _GPIO_H_
#define _GPIO_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdatomic.h>

int GpioMapRegs();
int GpioLightInit();
int GpioLightControl(int n, int val);
int GpioChassisInit();
int GpioChassisPwmControl(int val);
int GpioChassisDirection(uint8_t chassis1_dir, uint8_t chassis2_dir);
void GpioLightDispatcher();
void GpioChassisDispatcher();
int GpioCameraXYInit();
int GpioCameraXYSet(int x_pwm, int y_pwm);
void GpioCameraXYDispatcher();
//void PwmTest();

enum LIGHT_STATE
{
	LIGHT_OFF = 0,
	LIGHT_ON,
};

enum LIGHT_SRC
{
	LIGHT_CAMERA = 0,
	LIGHT_N,
};

enum CHASSIS_DIRECTION
{
	FORWARD = 0,
	BACK,
};

enum PIN_STATE
{
	LOW = 0,
	HIGH,
};



#endif