#include "tcp_server.h"
#include "fifo.h"
#include "gpio.h"
#include "ads1115.h"

#define I2C_DEV     "/dev/i2c-1"
#define ADS_ADDR    0x48

#define MAIN_SLOT_MAX_CNT  24
#define MAIN_SLOT_MAX_SIZE 256

#define CMD_ARGS_CNT		2
// server -p 8081

SLOT_T tcp_fifo_slots;
FIFO_T tcp_fifo_recv;
FIFO_T tcp_fifo_send;
FIFO_T light_fifo;
FIFO_T chassis_fifo;
FIFO_T camera_xy_fifo;
FIFO_T battery_fifo;

int server_port;

atomic_int flag_stop = 0;

void *tcp_server_thread(void *arg) 
{
	
	printf("[%s], start..\n", __func__);
	
	TcpRecvDispatcher();

    printf("[%s], stop..\n", __func__);
	
    return NULL;
}

/*---------------------------------------------------------------------------------------------*/

void *tcp_answer_thread(void *arg) 
{
	
	printf("[%s], start..\n", __func__);
	
	TcpSendDispatcher();

    printf("[%s], stop..\n", __func__);
	
    return NULL;
}

/*---------------------------------------------------------------------------------------------*/

void *camera_light_thread(void *arg) 
{
	
	printf("[%s], start..\n", __func__);
	
	GpioLightDispatcher();

    printf("[%s], stop..\n", __func__);
	
    return NULL;
}

/*---------------------------------------------------------------------------------------------*/

void *chassis_thread(void *arg) 
{
	
	printf("[%s], start..\n", __func__);
	
	GpioChassisDispatcher();

    printf("[%s], stop..\n", __func__);
	
    return NULL;
}

/*---------------------------------------------------------------------------------------------*/

void *camera_xy_thread(void *arg) 
{
	
	printf("[%s], start..\n", __func__);
	
	GpioCameraXYDispatcher();

    printf("[%s], stop..\n", __func__);
	
    return NULL;
}

/*---------------------------------------------------------------------------------------------*/

void *adc_battery_thread(void *arg) 
{
	
	printf("[%s], start..\n", __func__);
	
	ADC_BatteryDispatcher();

    printf("[%s], stop..\n", __func__);
	
    return NULL;
}

/*---------------------------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
	pthread_t thread_server, thread_answ, thread_light, thread_chassis, thread_camera_xy, thread_battery;
	char *ptr_server_port;
	int opt;
	int ret;
	
	if(argc != CMD_ARGS_CNT + 1){
		printf("[%s], Usage: %s -p <port>\n", __func__, argv[0]);
		return 0;
	}
	
	while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
			case 'p':
				ptr_server_port = optarg;
				break;
			default:
				printf("[%s], Usage: %s -p <port>\n", __func__, argv[0]);
				return 0;
		}
    }
	
	server_port = atoi(ptr_server_port);
	if(server_port < 1 || server_port > 65535){
		printf("[%s], wrong server port format %s\n", __func__, ptr_server_port);
		return 0;
	}
	
	// all fifo init
	ret = FifoCreateSlots(&tcp_fifo_slots, MAIN_SLOT_MAX_CNT, MAIN_SLOT_MAX_SIZE);
	if(ret != 0)
		return -1;
	
	ret = FifoInit(&tcp_fifo_recv);
	if(ret != 0){
		return -1;
	}
	
	ret = FifoInit(&tcp_fifo_send);
	if(ret != 0){
		return -1;
	}
	
	ret = FifoInit(&light_fifo);
	if(ret != 0){
		return -1;
	}
	
	ret = FifoInit(&chassis_fifo);
	if(ret != 0){
		return -1;
	}
	
	ret = FifoInit(&camera_xy_fifo);
	if(ret != 0){
		return -1;
	}
	
	ret = FifoInit(&battery_fifo);
	if(ret != 0){
		return -1;
	}
	
	ret = FifoFill(&tcp_fifo_recv, &tcp_fifo_slots, MAIN_SLOT_MAX_CNT);
	if(ret != 0){
		return -1;
	}
	
	ret = ADC_Init(I2C_DEV, ADS_ADDR);
	if(ret != 0){
		return -1;
	}
	
	// map gpio registers into user space
	ret = GpioMapRegs();
	if(ret != 0){
		return -1;
	}
	
	ret = GpioLightInit();
	if(ret != 0){
		return -1;
	}
	
	ret = GpioChassisInit();
	if(ret != 0){
		return -1;
	}
	
	ret = GpioCameraXYInit();
	if(ret != 0){
		return -1;
	}
	
    ret = pthread_create(&thread_server, NULL, tcp_server_thread, NULL);
	if(ret != 0){
		printf("[%s], can't create server thread \n", __func__);
		return -1;
	}
	
	ret = pthread_create(&thread_answ, NULL, tcp_answer_thread, NULL);
	if(ret != 0){
		printf("[%s], can't create answer thread \n", __func__);
		atomic_store(&flag_stop, 1);
		pthread_join(thread_server, NULL);
		return -1;
	}
	
	ret = pthread_create(&thread_light, NULL, camera_light_thread, NULL);
	if(ret != 0){
		printf("[%s], can't create light thread \n", __func__);
		atomic_store(&flag_stop, 1);
		pthread_join(thread_server, NULL);
		pthread_join(thread_answ, NULL);
		return -1;
	}

	ret = pthread_create(&thread_chassis, NULL, chassis_thread, NULL);
	if(ret != 0){
		printf("[%s], can't create chassis thread \n", __func__);
		atomic_store(&flag_stop, 1);
		pthread_join(thread_server, NULL);
		pthread_join(thread_answ, NULL);
		pthread_join(thread_light, NULL);
		return -1;
	}
	
	ret = pthread_create(&thread_camera_xy, NULL, camera_xy_thread, NULL);
	if(ret != 0){
		printf("[%s], can't create camera xy thread \n", __func__);
		atomic_store(&flag_stop, 1);
		pthread_join(thread_server, NULL);
		pthread_join(thread_answ, NULL);
		pthread_join(thread_light, NULL);
		pthread_join(thread_chassis, NULL);
		return -1;
	}
	
	ret = pthread_create(&thread_battery, NULL, adc_battery_thread, NULL);
	if(ret != 0){
		printf("[%s], can't create battery thread \n", __func__);
		atomic_store(&flag_stop, 1);
		pthread_join(thread_server, NULL);
		pthread_join(thread_answ, NULL);
		pthread_join(thread_light, NULL);
		pthread_join(thread_chassis, NULL);
		pthread_join(thread_camera_xy, NULL);
		return -1;
	}

    pthread_join(thread_server, NULL);
	pthread_join(thread_answ, NULL);
	pthread_join(thread_light, NULL);
	pthread_join(thread_chassis, NULL);
	pthread_join(thread_camera_xy, NULL);
	pthread_join(thread_battery, NULL);
	
	printf("stopped main\n");

	return 0;
}