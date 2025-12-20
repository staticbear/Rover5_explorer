#include "tcp_server.h"
#include "fifo.h"
#include "gpio.h"

#define MAIN_SLOT_MAX_CNT  16
#define MAIN_SLOT_MAX_SIZE 256

SLOT_T net_fifo_slots;
FIFO_T net_fifo;
FIFO_T light_fifo;
FIFO_T chassis_fifo;
FIFO_T camera_xy_fifo;

atomic_int flag_stop = 0;

void *tcp_server_thread(void *arg) {
	
	printf("[%s], start..\n", __func__);
	
	TcpDispatcher();

    printf("[%s], stop..\n", __func__);
	
    return NULL;
}

/*---------------------------------------------------------------------------------------------*/

void *camera_light_thread(void *arg) {
	
	printf("[%s], start..\n", __func__);
	
	GpioLightDispatcher();

    printf("[%s], stop..\n", __func__);
	
    return NULL;
}

/*---------------------------------------------------------------------------------------------*/

void *chassis_thread(void *arg) {
	
	printf("[%s], start..\n", __func__);
	
	GpioChassisDispatcher();

    printf("[%s], stop..\n", __func__);
	
    return NULL;
}

/*---------------------------------------------------------------------------------------------*/

void *camera_xy_thread(void *arg) {
	
	printf("[%s], start..\n", __func__);
	
	GpioCameraXYDispatcher();

    printf("[%s], stop..\n", __func__);
	
    return NULL;
}


/*---------------------------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
	pthread_t thread_net, thread_light, thread_chassis, thread_camera_xy;
	int ret;
	
	// all fifo init
	ret = FifoCreateSlots(&net_fifo_slots, MAIN_SLOT_MAX_CNT, MAIN_SLOT_MAX_SIZE);
	if(ret != 0)
		return -1;
	
	ret = FifoInit(&net_fifo);
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
	
	ret = FifoFill(&net_fifo, &net_fifo_slots, MAIN_SLOT_MAX_CNT);
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
	
    ret = pthread_create(&thread_net, NULL, tcp_server_thread, NULL);
	if(ret != 0){
		printf("[%s], can't create net thread \n", __func__);
		return -1;
	}
	
	ret = pthread_create(&thread_light, NULL, camera_light_thread, NULL);
	if(ret != 0){
		printf("[%s], can't create light thread \n", __func__);
		atomic_store(&flag_stop, 1);
		pthread_join(thread_net, NULL);
		return -1;
	}

	ret = pthread_create(&thread_chassis, NULL, chassis_thread, NULL);
	if(ret != 0){
		printf("[%s], can't create light thread \n", __func__);
		atomic_store(&flag_stop, 1);
		pthread_join(thread_net, NULL);
		pthread_join(thread_light, NULL);
		return -1;
	}
	
	ret = pthread_create(&thread_camera_xy, NULL, camera_xy_thread, NULL);
	if(ret != 0){
		printf("[%s], can't create light thread \n", __func__);
		atomic_store(&flag_stop, 1);
		pthread_join(thread_net, NULL);
		pthread_join(thread_light, NULL);
		pthread_join(thread_chassis, NULL);
		return -1;
	}

    pthread_join(thread_net, NULL);
	pthread_join(thread_light, NULL);
	pthread_join(thread_chassis, NULL);
	pthread_join(thread_camera_xy, NULL);
	
	printf("stopped main\n");

	return 0;
}