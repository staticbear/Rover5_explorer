#include "tcp_client.h"
#include "client.h"
#include "fifo.h"

extern FIFO_T tcp_fifo_recv;
extern FIFO_T tcp_chassis_fifo;
extern FIFO_T tcp_battery_fifo;
extern atomic_int flag_stop;
int sock = 0;

/*---------------------------------------------------------------------------------------------*/

int check_ip_addr(const char *ip) 
{
    struct sockaddr_in serv_addr;
    return inet_pton(AF_INET, ip, &serv_addr.sin_addr);
}

/*---------------------------------------------------------------------------------------------*/

int Tcp_Connect(char *server_ip, int port)
{
	int ret;
    struct sockaddr_in serv_addr;
	struct timeval tv;
	tv.tv_sec = 3;   
	tv.tv_usec = 0;
	
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("[%s], socket error\n", __func__);
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        printf("[%s], inet_pton error\n", __func__);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("[%s], connect error\n", __func__);
        return -1;
    }
	
	ret = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	if (ret < 0) {	
		Tcp_Close();
		printf("[%s], setsockopt return %d\n", __func__, ret);
		return -1;
	}
	
	return 0;
}

/*---------------------------------------------------------------------------------------------*/

void Tcp_Close()
{
	if(sock){
		close(sock);
		sock = 0;
	}
	return;
}

/*---------------------------------------------------------------------------------------------*/

void TcpRecvDispatcher()
{
	int ret;
	
	while(!flag_stop)
	{
		int rd_size;
		
		NODE_T *item = FifoPop(&tcp_fifo_recv);
		if(!item){
			printf("[%s], no free slots... \n", __func__);
			sleep(1);
			continue;
		}
	
		rd_size = read(sock, item->ptr_mem, item->item_size);
		if(rd_size < 0){
			Tcp_Close();
			atomic_store(&flag_stop, 1);
			printf("[%s], read timeout, connection closed \n", __func__);
			break;
		}
		else if(rd_size == 0)
		{
			Tcp_Close();
			atomic_store(&flag_stop, 1);
			printf("[%s], connection closed by server \n", __func__);
			break;
		}
		else{
			COMMON_PACKET_T *client_packet = (COMMON_PACKET_T *)item->ptr_mem;
			uint8_t *ptr_client_data = (uint8_t *)client_packet->ptr_data;
			uint16_t crc = client_packet->packet_id + client_packet->packet_number + client_packet->data_size;
				
			for(int i = 0; i < client_packet->data_size; i++)
				crc += ptr_client_data[i];
			
			if(client_packet->crc == crc){
				if(client_packet->packet_id == BATTERY_PACKET){
					ret = FifoPush(&tcp_battery_fifo, item);
				}
				else if(client_packet->packet_id == CHASSIS_PACKET){
					ret = FifoPush(&tcp_chassis_fifo, item);
				}
				else{
					printf("[%s],unknown packet id = %d\n", __func__, client_packet->packet_id);
					client_packet->ptr_data[0] = REQUEST_UNKNOW_PCKT_ID;
					ret = FifoPush(&tcp_fifo_recv, item);
				}
			}
			else{
				printf("[%s], wrong crc, received = %x, calculated = %x\n", __func__, client_packet->crc, crc);
				client_packet->ptr_data[0] = REQUEST_WRONG_CRC;
				ret = FifoPush(&tcp_fifo_recv, item);
			}
			
			if(ret != 0)
			{
				Tcp_Close();
				printf("[%s], FifoPush error, stop everything..\n", __func__);
				atomic_store(&flag_stop, 1);
			}
		}
	}
	
	return;
}

/*---------------------------------------------------------------------------------------------*/

void TcpBatteryDispatcher()
{
	uint8_t buffer_send[256];
	uint32_t packet_number = 0;
	int ret;
	
	while(!flag_stop)
	{
		COMMON_PACKET_T *snd_pack = (COMMON_PACKET_T *)buffer_send;
		snd_pack->packet_id = BATTERY_PACKET;
		snd_pack->packet_number = packet_number;
		snd_pack->data_size = 0;
		snd_pack->crc = snd_pack->packet_id + snd_pack->packet_number + snd_pack->data_size;

		ret = send(sock, (void *)&buffer_send, sizeof(COMMON_PACKET_T), 0);
		if(ret == -1){
			printf("[%s], send return -1\n", __func__);
			usleep(1000000);
			continue;
		}

		NODE_T *item = 0;
		
		while(1)
		{
			if(flag_stop)
				return;
			
			item = FifoPop(&tcp_battery_fifo);
			if(item)
				break;
			else
				usleep(10000);
		}
		
		COMMON_PACKET_T *common_packet = (COMMON_PACKET_T *)item->ptr_mem;
		BATTERY_DATA_T *battery_data = (BATTERY_DATA_T *)common_packet->ptr_data;
		
		if(common_packet->packet_number == packet_number)
		{
			if(battery_data->status == 0){
				double voltage = battery_data->raw_data * 6.144 / 32768.0;
				printf("[%s], Voltage AIN0 = %.4f V\n", __func__, voltage);
			}
			else
				printf("[%s], error in battery measure = %d\n", __func__, battery_data->status);
		}
		else
			printf("[%s], wrong packet number expected number = %d, received number = %d\n", __func__, packet_number, common_packet->packet_number);
		
		ret = FifoPush(&tcp_fifo_recv, item);
		if(ret != 0){
			printf("[%s], can't push into tcp_fifo_send, stop everything..\n", __func__);
			atomic_store(&flag_stop, 1);
		}
		
		packet_number++;
		
		usleep(1000000);
	}
	
	return;
}