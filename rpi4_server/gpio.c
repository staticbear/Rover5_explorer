#include "server.h"
#include "gpio.h"
#include "fifo.h"

#define PWM_BASE  0xFE20C000
#define GPIO_BASE 0xFE200000

#define GPFSEL1     (0x04 / 4)
#define GPFSEL2     (0x08 / 4)
#define GPSET0		(0x1C / 4)
#define GPCLR0		(0x28 / 4)
#define PUP_PDN_CNTRL_REG0	(0xE4 / 4)
#define PUP_PDN_CNTRL_REG1	(0xE8 / 4)

#define PWM_CTL     (0x00 / 4)
#define PWM_RNG1    (0x10 / 4)
#define PWM_DAT1    (0x14 / 4)
#define PWM_RNG2    (0x20 / 4)
#define PWM_DAT2    (0x24 / 4)

#define BLOCK_SIZE (4*1024)

#define PWM_MAX 		10
#define ONE_SEC_1000US	1000

#define CAMERA_XY_RANGE 1200000
#define CAMERA_XY_MIN	30000

extern FIFO_T tcp_fifo_send;
extern FIFO_T light_fifo;
extern FIFO_T chassis_fifo;
extern FIFO_T camera_xy_fifo;
extern atomic_int flag_stop;

volatile uint32_t *pwm = 0;
volatile uint32_t *gpio = 0;

int GpioMapRegs()
{
	int mem_fd;
    void *gpio_map, *pwm_map;
	
	if((mem_fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
        printf("[%s], can't open /dev/mem\n", __func__);
        return -1;
    }
	
	 // map registers to  app virtual space
    gpio_map = mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, GPIO_BASE);
    pwm_map  = mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, PWM_BASE);

	close(mem_fd);
	
    if(gpio_map == MAP_FAILED || pwm_map == MAP_FAILED) {
        printf("[%s], gpio_map == MAP_FAILED || pwm_map == MAP_FAILED \n", __func__);
        return -1;
    }
	
	gpio = (volatile uint32_t *)gpio_map;
    pwm  = (volatile uint32_t *)pwm_map;

	return 0;
}

/*---------------------------------------------------------------------------------------------*/

int GpioLightInit()
{
	if(!gpio){
		printf("[%s], gpio == 0\n", __func__);
		return -1;
	}
	
	// init gpio16 as output 
    gpio[GPFSEL1] &= ~(7 << 18);  // clear bits [20:18], FSEL16 = 0
    gpio[GPFSEL1] |=  (1 << 18);  // FSEL16 = 1 = 001 output
	gpio[PUP_PDN_CNTRL_REG1] &= ~(3 << 0);	// no pull up/down resistor
	gpio[GPCLR0] = (1 << 16);
	
	return 0;
}

/*---------------------------------------------------------------------------------------------*/

int GpioChassisInit()
{
	if(!gpio){
		printf("[%s], gpio == 0\n", __func__);
		return -1;
	}
	
	// init gpio17,gpio18,gpio19,gpio20,gpio21 as output 
    gpio[GPFSEL1] &= ~(7 << 21);  // clear bits [23:21], FSEL17 = 0
	gpio[GPFSEL1] &= ~(7 << 24);  // clear bits [26:24], FSEL18 = 0
	gpio[GPFSEL1] &= ~(7 << 27);  // clear bits [29:27], FSEL19 = 0
	gpio[GPFSEL1] |=  (1 << 21);  // FSEL17 = 1 = 001 output
	gpio[GPFSEL1] |=  (1 << 24);  // FSEL18 = 1 = 001 output
	gpio[GPFSEL1] |=  (1 << 27);  // FSEL19 = 1 = 001 output
	gpio[GPFSEL2] &= ~(7 << 0);  // clear bits [2:0], FSEL20 = 0
	gpio[GPFSEL2] &= ~(7 << 3);  // clear bits [5:3], FSEL21 = 0
	gpio[GPFSEL2] |=  (1 << 0);  // FSEL20 = 1 = 001 output
	gpio[GPFSEL2] |=  (1 << 3);  // FSEL21 = 1 = 001 output
	
	gpio[PUP_PDN_CNTRL_REG1] &= ~(3 << 10);	// NTRL21 no pull up/down resistor
	gpio[PUP_PDN_CNTRL_REG1] &= ~(3 << 8);	// NTRL20 no pull up/down resistor
	gpio[PUP_PDN_CNTRL_REG1] &= ~(3 << 6);	// NTRL19 no pull up/down resistor
	gpio[PUP_PDN_CNTRL_REG1] &= ~(3 << 4);	// NTRL18 no pull up/down resistor
	gpio[PUP_PDN_CNTRL_REG1] &= ~(3 << 2);	// NTRL17 no pull up/down resistor
	
	gpio[GPCLR0] = (1 << 17) | (1 << 18) | (1 << 19) | (1 << 20) | (1 << 21) ;
	
	return 0;
}

/*---------------------------------------------------------------------------------------------*/

int GpioLightControl(int n, int val)
{
	int i;
	
	if(!gpio){
		printf("[%s], gpio == 0 \n", __func__);
		return -1;
	}
	
	if(n < 0 || n >= LIGHT_N){
		printf("[%s], wrong light n = %d \n", __func__, n);
		return -1;
	}
	
	i = (n == LIGHT_CAMERA) ? 16 : 0;
	
	if(val == LIGHT_OFF)
		gpio[GPCLR0] = (1 << i);
	else if(val == LIGHT_ON)
		gpio[GPSET0] = (1 << i);
	else{
		printf("[%s], wrong val = %d \n", __func__, val);
		return -1;
	}
	
	return 0;
}

/*---------------------------------------------------------------------------------------------*/

int GpioChassisPwmControl(int val)
{
	if(!gpio){
		printf("[%s], gpio == 0 \n", __func__);
		return -1;
	}
	
	if(val == LOW)
		gpio[GPCLR0] = (1 << 17);
	else if(val == HIGH)
		gpio[GPSET0] = (1 << 17);
	else{
		printf("[%s], wrong val = %d \n", __func__, val);
		return -1;
	}
	
	return 0;
}

/*---------------------------------------------------------------------------------------------*/

int GpioChassisDirection(uint8_t chassis1_dir, uint8_t chassis2_dir)
{
	if(!gpio){
		printf("[%s], gpio == 0 \n", __func__);
		return -1;
	}
	
	if(chassis1_dir != FORWARD && chassis1_dir != BACK)
	{
		printf("[%s], wrong chassis1_dir = %d \n", __func__, chassis1_dir);
		return -1;
	}
	
	if(chassis2_dir != FORWARD && chassis2_dir != BACK)
	{
		printf("[%s], wrong chassis2_dir = %d \n", __func__, chassis2_dir);
		return -1;
	}
	
	if(chassis1_dir == FORWARD){
		gpio[GPSET0] = (1 << 18);
		gpio[GPCLR0] = (1 << 19);
	}
	else{
		gpio[GPSET0] = (1 << 19);
		gpio[GPCLR0] = (1 << 18);
	}
	
	if(chassis2_dir == FORWARD){
		gpio[GPSET0] = (1 << 20);
		gpio[GPCLR0] = (1 << 21);
	}
	else{
		gpio[GPSET0] = (1 << 21);
		gpio[GPCLR0] = (1 << 20);
	}
	
	return 0;
}

/*---------------------------------------------------------------------------------------------*/

int GpioCameraXYInit()
{
	if(!gpio || !pwm){
		printf("[%s], gpio == 0 || pwm == 0 \n", __func__);
		return -1;
	}
	
	// init gpio12, gpio13 as pwm 
    gpio[GPFSEL1] &= ~((7 << 6) | (7 << 9));  // clear bits
    gpio[GPFSEL1] |=  (4 << 6) | (4 << 9);    // ALT0 = 100 (PWM0)
	gpio[PUP_PDN_CNTRL_REG0] &= ~((3 << 26) | (3 << 24));	// no pull up/down resistor
	
    pwm[PWM_RNG1] = CAMERA_XY_RANGE;       // 50 Hz
    pwm[PWM_DAT1] = CAMERA_XY_MIN;   	   // 1ms
	pwm[PWM_RNG2] = CAMERA_XY_RANGE;       // 50 Hz
    pwm[PWM_DAT2] = CAMERA_XY_MIN;   	   // 1ms
	
    pwm[PWM_CTL]  = (1 << 0) |	   // PWEN1
	                (1 << 7) |	   // MSEN1
					(1 << 8) |     // PWEN2
					(1 << 15);	   // MSEN2
					
	return 0;
}

/*---------------------------------------------------------------------------------------------*/

int GpioCameraXYSet(int x_pwm, int y_pwm)
{
	
	if(!gpio || !pwm){
		printf("[%s], gpio == 0 || pwm == 0 \n", __func__);
		return -1;
	}
	
	if(x_pwm < CAMERA_XY_MIN || x_pwm > CAMERA_XY_RANGE){
		printf("[%s], wrong x_pwm = %d \n", __func__, x_pwm);
		return -1;
	}
	
	if(y_pwm < CAMERA_XY_MIN || y_pwm > CAMERA_XY_RANGE){
		printf("[%s], wrong y_pwm = %d \n", __func__, y_pwm);
		return -1;
	}
	
	pwm[PWM_DAT1] = x_pwm;   
	pwm[PWM_DAT2] = y_pwm;  
	
	return 0;
}

/*---------------------------------------------------------------------------------------------*/
/*
void PwmTest()
{
	int n = 0;
	
	if(!gpio || !pwm){
		printf("[%s], gpio == 0 || pwm == 0 \n", __func__);
		return;
	}
	
	// init gpio12, gpio13 as pwm 
    gpio[GPFSEL1] &= ~((7 << 6) | (7 << 9));  // clear bits
    gpio[GPFSEL1] |=  (4 << 6) | (4 << 9);    // ALT0 = 100 (PWM0)
	gpio[PUP_PDN_CNTRL_REG0] &= ~((3 << 26) | (3 << 24));	// no pull up/down resistor
	
	// pwm settings
	printf("pwm settings \n");
    pwm[PWM_RNG1] = 1200000;       // 50 Hz
    pwm[PWM_DAT1] = 30000;   	   // 1ms
	pwm[PWM_RNG2] = 1200000;       // 50 Hz
    pwm[PWM_DAT2] = 30000;   	   // 1ms
	
    pwm[PWM_CTL]  = (1 << 0) |	   // PWEN1
	                (1 << 7) |	   // MSEN1
					(1 << 8) |     // PWEN2
					(1 << 15);	   // MSEN2
	
	while (n < 10){
		pwm[PWM_DAT1] = 30000;   
		pwm[PWM_DAT2] = 30000;  
		sleep(2);
		pwm[PWM_DAT1] = 120000; 
		pwm[PWM_DAT2] = 120000; 
		sleep(2);
		n++;
	}
	
	return;
}
*/

/*---------------------------------------------------------------------------------------------*/

void GpioCameraXYDispatcher()
{
	int ret;
	
	while(!flag_stop)
	{
		NODE_T *item = FifoPop(&camera_xy_fifo);
		if(!item){
			usleep(100000);
			continue;
		}
		
		COMMON_PACKET_T *common_packet = (COMMON_PACKET_T *)item->ptr_mem;
		
		if(common_packet->data_size == sizeof(CAMERA_XY_DATA_T))
		{
			CAMERA_XY_DATA_T *camara_xy_data = (CAMERA_XY_DATA_T*)common_packet->ptr_data;
			ret = GpioCameraXYSet(camara_xy_data->x_pwm, camara_xy_data->y_pwm);
			if(ret != 0)
				common_packet->ptr_data[0] = REQUEST_OK | ANSW_CAMERA_XY_SET_ERR;
			else
				common_packet->ptr_data[0] = REQUEST_OK;
		}
		
		common_packet->data_size = 1;
		
		ret = FifoPush(&tcp_fifo_send, item);
		if(ret != 0){
			printf("[%s], can't push into tcp_fifo_send, stop everything..\n", __func__);
			atomic_store(&flag_stop, 1);
			break;
		}
	}
	
	return;
}

/*---------------------------------------------------------------------------------------------*/

void GpioLightDispatcher()
{
	int ret;
	
	while(!flag_stop)
	{
		NODE_T *item = FifoPop(&light_fifo);
		if(!item){
			usleep(100000);
			continue;
		}
		
		COMMON_PACKET_T *common_packet = (COMMON_PACKET_T *)item->ptr_mem;
		
		if(common_packet->data_size != 0 && (common_packet->data_size % sizeof(LIGHT_DATA_T)) == 0)
		{
			int array_cnt = common_packet->data_size / sizeof(LIGHT_DATA_T);
			LIGHT_DATA_T *ligh_data_array = (LIGHT_DATA_T*)common_packet->ptr_data;
			for(int i = 0; i < array_cnt; i++)
			{
				ret = GpioLightControl(ligh_data_array[i].cam_id, ligh_data_array[i].cam_val);
				if(ret != 0){
					printf("[%s], GpioLightControl error, i = %d\n", __func__, i);
					common_packet->ptr_data[0] = REQUEST_OK | ANSW_LIGHT_CTRL_ERR;
					break;
				}
				common_packet->ptr_data[0] = REQUEST_OK;
			}
		}
		else{
			printf("[%s], problem with data size = %d\n", __func__, common_packet->data_size);
			common_packet->ptr_data[0] = REQUEST_OK | ANSW_LIGHT_WRND_SIZE;
		}
		
		common_packet->data_size = 1;
		
		ret = FifoPush(&tcp_fifo_send, item);
		if(ret != 0){
			printf("[%s], can't push into tcp_fifo_send, stop everything..\n", __func__);
			atomic_store(&flag_stop, 1);
			break;
		}
	}
	return;
}

/*---------------------------------------------------------------------------------------------*/

void GpioChassisDispatcher()
{
	int ret;
	uint8_t answ_code;
	uint32_t no_cmd_cnt = 0;
	int pwm_cnt = 0;
	int pwm_pol_cnt = 0;
	int pwm_pol_upd = 0;
	int pwm_pin_state = LOW;
	NODE_T *item = 0;
	COMMON_PACKET_T *common_packet;
	
	while(!flag_stop)
	{
		if(pwm_cnt == PWM_MAX){
			
			item = FifoPop(&chassis_fifo);
			if(item){
				
				common_packet = (COMMON_PACKET_T *)item->ptr_mem;
				CHASSIS_DATA_T *chassis_data = (CHASSIS_DATA_T*)common_packet->ptr_data;
				
				answ_code = REQUEST_OK;
				
				//set chassis direction
				if(chassis_data->pwm_pol > PWM_MAX || chassis_data->pwm_pol < 0){
					printf("[%s], wrong pwm value = %d\n", __func__, chassis_data->pwm_pol);
					answ_code |= ANSW_CHASSIS_WRNG_PWM;
					pwm_pol_cnt = 0;
				}
				else
					pwm_pol_cnt = chassis_data->pwm_pol;
				
				ret = GpioChassisDirection(chassis_data->chassis1_dir, chassis_data->chassis2_dir);
				if(ret != 0)
					answ_code |= ANSW_CHASSIS_DIR_ERR;
				
				no_cmd_cnt = 0;
				pwm_pol_upd = pwm_pol_cnt;
			}
			else{
				no_cmd_cnt++;
				pwm_pol_cnt = pwm_pol_upd;
			}
			
			if(pwm_pol_cnt){
				pwm_pin_state = HIGH;
				if(GpioChassisPwmControl(HIGH) != 0)
					answ_code |= ANSW_CHASSIS_PWM_H_ERR;
			}
			else{
				pwm_pin_state = LOW;
				if(GpioChassisPwmControl(LOW) != 0)
					answ_code |= ANSW_CHASSIS_PWM_L_ERR;
			}
			pwm_cnt = 0;
		}
		else{
			
			if(pwm_pol_cnt)
				pwm_pol_cnt--;
			else if(pwm_pin_state == HIGH){
				pwm_pin_state = LOW;
				GpioChassisPwmControl(LOW);
			}
				
			pwm_cnt++;
		}
		
		
		// push answer
		if(item){
			common_packet->ptr_data[0] = answ_code;
			common_packet->data_size = 1;
			ret = FifoPush(&tcp_fifo_send, item);
			if(ret != 0){
				printf("[%s], can't push into tcp_fifo_send, stop everything..\n", __func__);
				atomic_store(&flag_stop, 1);
				break;
			}
			item = 0;
		}
		
		// if there are no commands in one seconds stop motor
		if(no_cmd_cnt == ONE_SEC_1000US && pwm_pin_state == HIGH){
			pwm_pol_upd = 0;
			pwm_pol_cnt = 0;
			pwm_pin_state == LOW;
			GpioChassisPwmControl(LOW);
		}
		
		usleep(1000);
	}
	
	GpioChassisPwmControl(LOW);
	
	return;
}