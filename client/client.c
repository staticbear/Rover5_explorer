#include "client.h"
#include "fifo.h"
#include "tcp_client.h"
#include "joystick.h"


#define MAIN_SLOT_MAX_CNT  24
#define MAIN_SLOT_MAX_SIZE 256

#define CMD_ARGS_CNT 6
// client -s 192.168.0.101 -p 8081 -j /dev/input/js0

SLOT_T tcp_fifo_slots;
SLOT_T joy_fifo_slots;
FIFO_T tcp_fifo_recv;
FIFO_T joystick_fifo;
FIFO_T joy_chassis_fifo;
FIFO_T tcp_chassis_fifo;
FIFO_T tcp_battery_fifo;

atomic_int flag_stop = 0;

/*---------------------------------------------------------------------------------------------*/

void *tcp_recv_thread(void *arg) 
{
	
	printf("[%s], start..\n", __func__);
	
	TcpRecvDispatcher();

    printf("[%s], stop..\n", __func__);
	
    return NULL;
}

/*---------------------------------------------------------------------------------------------*/

void *joystick_poll_thread(void *arg) 
{
	printf("[%s], start..\n", __func__);
	
	JoystickPollDispatcher();
	
	printf("[%s], stop..\n", __func__);
	
	return NULL;
}

/*---------------------------------------------------------------------------------------------*/

void *chassis_thread(void *arg) 
{
	printf("[%s], start..\n", __func__);
	
	ChassisDispatcher();
	
	printf("[%s], stop..\n", __func__);
	
	return NULL;
}

/*---------------------------------------------------------------------------------------------*/

void *tcp_battery_thread(void *arg) 
{
	
	printf("[%s], start..\n", __func__);
	
	TcpBatteryDispatcher();

    printf("[%s], stop..\n", __func__);
	
    return NULL;
}

/*---------------------------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
	int ret;
	int opt;
	char *ptr_server_ip;
	char *ptr_server_port;
	char *ptr_joystick_path;
	int server_port;
	pthread_t thread_recv, thread_joystick, thread_chassis, thread_battery;
	
	if(argc != CMD_ARGS_CNT + 1){
		printf("[%s], Usage: %s -s <ip> -p <port> -j <joystick path>\n", __func__, argv[0]);
		return 0;
	}
	
	while ((opt = getopt(argc, argv, "s:p:j:")) != -1) {
        switch (opt) {
			case 's':
				ptr_server_ip = optarg;
				break;
			case 'p':
				ptr_server_port = optarg;
				break;
			case 'j':
				ptr_joystick_path = optarg;
				break;
			default:
				printf("[%s], Usage: %s -s <ip> -p <port> -j <joystick path>\n", __func__, argv[0]);
				return 0;
		}
    }
	
	if(!ptr_server_ip || !ptr_server_port || !ptr_joystick_path){
		printf("[%s], Usage: %s -s <ip> -p <port> -j <joystick path>\n", __func__, argv[0]);
		return 0;
	}
	
	if(check_ip_addr(ptr_server_ip) != 1){
		printf("[%s], wrong server ip format %s\n", __func__, ptr_server_ip);
		return 0;
	}
	
	server_port = atoi(ptr_server_port);
	if(server_port < 1 || server_port > 65535){
		printf("[%s], wrong server port format %s\n", __func__, ptr_server_port);
		return 0;
	}
	
	// joystick
	ret = JoystickInit(ptr_joystick_path);
	if(ret != 0)
		return -1;
	
	// all fifo init
	ret = FifoCreateSlots(&tcp_fifo_slots, MAIN_SLOT_MAX_CNT, MAIN_SLOT_MAX_SIZE);
	if(ret != 0)
		return -1;
	
	ret = FifoCreateSlots(&joy_fifo_slots, MAIN_SLOT_MAX_CNT, MAIN_SLOT_MAX_SIZE);
	if(ret != 0)
		return -1;
	
	ret = FifoInit(&tcp_fifo_recv);
	if(ret != 0){
		return -1;
	}
	
	ret = FifoInit(&joystick_fifo);
	if(ret != 0){
		return -1;
	}
	
	ret = FifoInit(&joy_chassis_fifo);
	if(ret != 0){
		return -1;
	}
	
	ret = FifoInit(&tcp_chassis_fifo);
	if(ret != 0){
		return -1;
	}
	
	ret = FifoInit(&tcp_battery_fifo);
	if(ret != 0){
		return -1;
	}
	
	ret = FifoFill(&tcp_fifo_recv, &tcp_fifo_slots, MAIN_SLOT_MAX_CNT);
	if(ret != 0){
		return -1;
	}
	
	ret = FifoFill(&joystick_fifo, &joy_fifo_slots, MAIN_SLOT_MAX_CNT);
	if(ret != 0){
		return -1;
	}
	
	ret = Tcp_Connect(ptr_server_ip, server_port);
	if(ret != 0){
		return -1;
	}
	
	ret = pthread_create(&thread_recv, NULL, tcp_recv_thread, NULL);
	if(ret != 0){
		printf("[%s], can't create recv thread \n", __func__);
		return -1;
	}
	
	ret = pthread_create(&thread_joystick, NULL, joystick_poll_thread, NULL);
	if(ret != 0){
		printf("[%s], can't create joystick poll thread thread \n", __func__);
		atomic_store(&flag_stop, 1);
		pthread_join(thread_recv, NULL);
		return -1;
	}
	
	ret = pthread_create(&thread_chassis, NULL, chassis_thread, NULL);
	if(ret != 0){
		printf("[%s], can't create chassis thread \n", __func__);
		atomic_store(&flag_stop, 1);
		pthread_join(thread_recv, NULL);
		pthread_join(thread_joystick, NULL);
		return -1;
	}
	
	ret = pthread_create(&thread_battery, NULL, tcp_battery_thread, NULL);
	if(ret != 0){
		printf("[%s], can't create battery thread \n", __func__);
		atomic_store(&flag_stop, 1);
		pthread_join(thread_recv, NULL);
		pthread_join(thread_joystick, NULL);
		pthread_join(thread_chassis, NULL);
		return -1;
	}
	
	pthread_join(thread_recv, NULL);
	pthread_join(thread_joystick, NULL);
	pthread_join(thread_chassis, NULL);
	pthread_join(thread_battery, NULL);
	
	//printf("[%s], ip_addr: %s, port: %s\n", __func__, ptr_server_ip, ptr_server_port);
	return 0;
}