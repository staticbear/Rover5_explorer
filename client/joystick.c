#include "joystick.h"
#include "client.h"
#include "fifo.h"

#define JOY_AXIS_MAX		32767
#define JOY_AXIS_SPEED  	4
#define JOY_AXIS_TURN		3
#define AXIS_SPEED_DELTA_US 10000

int joystick_fd = 0;

extern int sock;
extern atomic_int flag_stop;
extern FIFO_T tcp_fifo_recv;
extern FIFO_T tcp_chassis_fifo;
extern FIFO_T joystick_fifo;
extern FIFO_T joy_chassis_fifo;
/*---------------------------------------------------------------------------------------------*/

int JoystickInit(char *path)
{
	joystick_fd = open(path, O_RDONLY);
    if (joystick_fd < 0) {
        printf("[%s], can't open joystick device %s\n", __func__, path);
        return -1;
    }
	
	return 0;
}

/*---------------------------------------------------------------------------------------------*/

void JoystickClose()
{
	if(joystick_fd){
		close(joystick_fd);
		joystick_fd = 0;
	}
	return;
}

/*---------------------------------------------------------------------------------------------*/

static inline uint64_t gettime_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t)ts.tv_sec * 1000000ULL +
           (uint64_t)ts.tv_nsec / 1000ULL;
}


void JoystickPollDispatcher()
{
	int ret;
	struct js_event event;
	struct pollfd pfd = {0};
	uint64_t axis_spd_time_prev, axis_spd_time_now;
	uint64_t axis_trn_time_prev, axis_trn_time_now;
	uint64_t axis_spd_time_dt = 0;
	uint64_t axis_trn_time_dt = 0;
	NODE_T *item_chassis_spd = 0;
	NODE_T *item_chassis_trn = 0;
	
	pfd.fd = joystick_fd;
	pfd.events = POLLIN;
	
	axis_spd_time_prev = gettime_us();
	axis_trn_time_prev = axis_spd_time_prev;
	
	while(!flag_stop)
	{
		ret = 0;
		
		if(axis_spd_time_dt >= AXIS_SPEED_DELTA_US)
		{
			if(item_chassis_spd){
				ret = FifoPush(&joy_chassis_fifo, item_chassis_spd);
				item_chassis_spd = 0;
				axis_spd_time_dt = 0;
			}
		}
		
		if(axis_trn_time_dt >= AXIS_SPEED_DELTA_US)
		{
			if(item_chassis_trn){
				ret = FifoPush(&joy_chassis_fifo, item_chassis_trn);
				item_chassis_trn = 0;
				axis_trn_time_dt = 0;
			}
		}
		
		if(ret != 0)
		{
			printf("[%s], FifoPush error, stop everything..\n", __func__);
			JoystickClose();
			atomic_store(&flag_stop, 1);
			break;
		}
		
		ret = poll(&pfd, 1, 100); 
		if(ret == 0) {
			axis_spd_time_now = gettime_us();
			axis_trn_time_now = axis_spd_time_now;
			axis_spd_time_dt += axis_spd_time_now - axis_spd_time_prev;
			axis_trn_time_dt += axis_trn_time_now - axis_trn_time_prev;
			axis_spd_time_prev = axis_spd_time_now;
			axis_trn_time_prev = axis_trn_time_now;
			continue;
		}
		
		int size = read(joystick_fd, &event, sizeof(event));
        if(size != sizeof(event)) {
			JoystickClose();
            atomic_store(&flag_stop, 1);
			printf("[%s], joystick read error \n", __func__);
			break;
        }

        if(event.type & JS_EVENT_INIT)
            continue;
		
		if(event.type & JS_EVENT_AXIS) {
			

			if(event.number != JOY_AXIS_SPEED && event.number != JOY_AXIS_TURN)
				continue;
			
			if(!item_chassis_spd && event.number == JOY_AXIS_SPEED){
				item_chassis_spd = FifoPop(&joystick_fifo);
				if(!item_chassis_spd){
						printf("[%s], no free slots... \n", __func__);
						usleep(10000);
						continue;
				}
			}
			
			if(!item_chassis_trn && event.number == JOY_AXIS_TURN){
				item_chassis_trn = FifoPop(&joystick_fifo);
				if(!item_chassis_trn){
						printf("[%s], no free slots... \n", __func__);
						usleep(10000);
						continue;
				}
			}
			
			if(event.number == JOY_AXIS_SPEED)
			{
				JOYSTICK_AXIS_DATA_T *joy_axis_data = (JOYSTICK_AXIS_DATA_T *)item_chassis_spd->ptr_mem;
				joy_axis_data->axis = JOY_AXIS_SPEED;
				joy_axis_data->axis_data = event.value;
				
				axis_spd_time_now = gettime_us();
				axis_spd_time_dt += axis_spd_time_now - axis_spd_time_prev;
				axis_spd_time_prev = axis_spd_time_now;
			}
			else if(event.number == JOY_AXIS_TURN)
			{
				JOYSTICK_AXIS_DATA_T *joy_axis_data = (JOYSTICK_AXIS_DATA_T *)item_chassis_trn->ptr_mem;
				joy_axis_data->axis = JOY_AXIS_TURN;
				joy_axis_data->axis_data = event.value;
				
				axis_trn_time_now = gettime_us();
				axis_trn_time_dt += axis_trn_time_now - axis_trn_time_prev;
				axis_trn_time_prev = axis_trn_time_now;
			}
			
			continue;
		}
		else if(event.type & JS_EVENT_BUTTON) {
			printf("Button %d %s\n", event.number, event.value ? "pressed" : "released");
        }	
		
		usleep(10000);
		
	}
	
	return;
}

/*---------------------------------------------------------------------------------------------*/

void ChassisDispatcher()
{
	int ret;
	uint8_t buffer_send[256];
	NODE_T *item;
	uint32_t packet_number = 0;
	
	int move_direction = FORWARD;
	int move_turn = DIRECT;
	
	COMMON_PACKET_T *snd_pack = (COMMON_PACKET_T *)buffer_send;
	CHASSIS_DATA_T *chassis_data = (CHASSIS_DATA_T *)snd_pack->ptr_data;
	uint8_t *ptr_data = snd_pack->ptr_data;
	
	snd_pack->packet_id = CHASSIS_PACKET;
	snd_pack->data_size = sizeof(CHASSIS_DATA_T);
	
	chassis_data->pwm_pol = 0;
	chassis_data->chassis1_dir = FORWARD;
	chassis_data->chassis2_dir = FORWARD;
	
	while(!flag_stop)
	{
		item = FifoPop(&joy_chassis_fifo);
		if(!item){
			usleep(1000);
			continue;
		}
		
		JOYSTICK_AXIS_DATA_T *joy_axis_data = (JOYSTICK_AXIS_DATA_T *)item->ptr_mem;
		
		if(joy_axis_data->axis == JOY_AXIS_SPEED){
			move_direction = (joy_axis_data->axis_data & (1 << 15)) ? FORWARD : BACK;
			if(move_direction == FORWARD){
				chassis_data->chassis1_dir = FORWARD;
				chassis_data->chassis2_dir = FORWARD;
			}
			else{
				chassis_data->chassis1_dir = BACK;
				chassis_data->chassis2_dir = BACK;
			}
		
			uint16_t pwm_pol = (joy_axis_data->axis_data & (1 << 15)) ? ~joy_axis_data->axis_data + 1 : joy_axis_data->axis_data;
			pwm_pol /= (JOY_AXIS_MAX / 10);
			chassis_data->pwm_pol = pwm_pol;
		}
		else{
			if(joy_axis_data->axis_data == 0)
				move_turn = DIRECT;
			else
				move_turn = (joy_axis_data->axis_data & (1 << 15)) ? LEFT : RIGHT;	
		}	
		
		
		if(move_turn == DIRECT)
		{
			if(move_direction == FORWARD){
				chassis_data->chassis1_dir = FORWARD;
				chassis_data->chassis2_dir = FORWARD;
			}
			else{
				chassis_data->chassis1_dir = BACK;
				chassis_data->chassis2_dir = BACK;
			}
		}
		else if((move_direction == FORWARD && move_turn == RIGHT) || (move_direction == BACK && move_turn == LEFT)){
			chassis_data->chassis1_dir = FORWARD;
			chassis_data->chassis2_dir = BACK;
		}
		else{
			chassis_data->chassis1_dir = BACK;
			chassis_data->chassis2_dir = FORWARD;
		}
			
		ret = FifoPush(&joystick_fifo, item);
		if(ret != 0){
			printf("[%s], FifoPush error, stop everything..\n", __func__);
			atomic_store(&flag_stop, 1);
			break;
		}
		
		snd_pack->packet_number = packet_number;
		snd_pack->crc = snd_pack->packet_id + snd_pack->packet_number + snd_pack->data_size;
		for(int i = 0; i < snd_pack->data_size; i++)
			snd_pack->crc += ptr_data[i];
		
		ret = send(sock, (void *)&buffer_send, sizeof(COMMON_PACKET_T) + sizeof(CHASSIS_DATA_T), 0);
		if(ret == -1){
			printf("[%s],axes send return -1\n", __func__);
			continue;
		}
			
		while(1)
		{
			if(flag_stop){
				JoystickClose();
				return;
			}
				
			item = FifoPop(&tcp_chassis_fifo);
			if(item)
				break;
			else
				usleep(1000);
		}
			
		COMMON_PACKET_T *common_packet = (COMMON_PACKET_T *)item->ptr_mem;
		if(common_packet->packet_number == packet_number)
		{
			uint8_t answ_code = common_packet->ptr_data[0];
			if(answ_code != REQUEST_OK)
				printf("[%s], chassis answer code = %d\n", __func__, answ_code);
		}
		else
			printf("[%s], wrong packet number expected number = %d, received number = %d\n", __func__, packet_number, common_packet->packet_number);
			
		ret = FifoPush(&tcp_fifo_recv, item);
		if(ret != 0){
			printf("[%s], FifoPush error, stop everything..\n", __func__);
			atomic_store(&flag_stop, 1);
			JoystickClose();
		}
		
		packet_number++;
	}
	return;
}
