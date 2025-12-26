#ifndef _ADS115_H_
#define _ADS115_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

int ADC_Init(char *i2c_addr, uint8_t dev_addr);
int ADC_ReadRawIN0(uint16_t *adc_raw);
void ADC_Close();
void ADC_BatteryDispatcher();

#endif