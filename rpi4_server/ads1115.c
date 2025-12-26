#include "server.h"
#include "fifo.h"
#include "ads1115.h"

#define REG_CONV    0x00
#define REG_CONFIG  0x01

extern FIFO_T battery_fifo;
extern FIFO_T tcp_fifo_send;
extern atomic_int flag_stop;

int adc_fd = 0;

int ADC_Init(char *i2c_addr, uint8_t dev_addr)
{
	int ret;
	uint8_t buf[3];
	
    adc_fd = open(i2c_addr, O_RDWR);
    if (adc_fd < 0) {
        printf("[%s], can't open %s \n", __func__, i2c_addr);
        return -1;
    }

    // setup adc i2c addr
    if (ioctl(adc_fd, I2C_SLAVE, dev_addr) < 0) {
        printf("[%s], can't set i2c addr %d \n", __func__, dev_addr);
		close(adc_fd);
		adc_fd = 0;
        return -1;
    }
	
	// configure ads115 in continuous mode, read IN0
    buf[0] = REG_CONFIG;
    buf[1] = 0xC0;   // MSB
    buf[2] = 0x83;   // LSB
    ret = write(adc_fd, buf, 3);
	if(ret != 3){
		printf("[%s], write return %d \n", __func__, ret);
		close(adc_fd);
		adc_fd = 0;
        return -1;
	}

    // wait first result
    usleep(8000);  

	return 0;
}

/*---------------------------------------------------------------------------------------------*/

int ADC_ReadRawIN0(uint16_t *adc_raw)
{
	int ret;
	uint8_t buf[2];
	
	if(adc_fd <= 0){
		printf("[%s], wrong descriptor %d \n", __func__, adc_fd);
        return -1;
	}		
	
    buf[0] = REG_CONV;
    ret = write(adc_fd, buf, 1);
	if(ret != 1){
		printf("[%s], write return %d \n", __func__, ret);
        return -1;
	}
	
    ret = read(adc_fd, buf, 2);
	if(ret != 2){
		printf("[%s], read return %d \n", __func__, ret);
        return -1;
	}
	
    *adc_raw = (buf[0] << 8) | buf[1];
	
	return 0;
}

/*---------------------------------------------------------------------------------------------*/

void ADC_Close()
{
	if(adc_fd <= 0){
		printf("[%s], wrong descriptor %d \n", __func__, adc_fd);
        return;
	}	
	close(adc_fd);	
	adc_fd = 0;
	
	return;
}

/*---------------------------------------------------------------------------------------------*/

void ADC_BatteryDispatcher()
{
	int ret;
	
	while(!flag_stop)
	{
		NODE_T *item = FifoPop(&battery_fifo);
		if(!item){
			usleep(100000);
			continue;
		}
		
		COMMON_PACKET_T *common_packet = (COMMON_PACKET_T *)item->ptr_mem;
		BATTERY_DATA_T *battery_data = (BATTERY_DATA_T *)common_packet->ptr_data;
		
		ret = ADC_ReadRawIN0(&battery_data->raw_data);
		battery_data->status = ret;
		common_packet->data_size = sizeof(BATTERY_DATA_T);
		
		ret = FifoPush(&tcp_fifo_send, item);
		if(ret != 0){
			printf("[%s], can't push into tcp_fifo_send, stop everything..\n", __func__);
			atomic_store(&flag_stop, 1);
			break;
		}
	}
	
	return;
}
