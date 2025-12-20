
#include "tcp_server.h"
#include "server.h"
#include "fifo.h"

extern FIFO_T net_fifo;
extern FIFO_T light_fifo;
extern FIFO_T chassis_fifo;
extern FIFO_T camera_xy_fifo;
extern atomic_int flag_stop;

int server_fd, client_fd;
struct sockaddr_in address = {0};
socklen_t addrlen = sizeof(address);

char server_answer_buf[256];

int CreateTcpServer()
{
	int ret;
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(server_fd == -1) {
        printf("[%s] socket return -1\n", __func__);
        return -1;
    }

	address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
	ret = bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    if(ret < 0) {
        printf("[%s], bind return %d\n", __func__, ret);
		close(server_fd);
        return -1;
    }
	
	ret = listen(server_fd, 1);
	if(ret < 0) {
        printf("[%s], listen return %d\n", __func__, ret);
		close(server_fd);
        return -1;
    }

    printf("[%s], Server is listening on port %d...\n", __func__, PORT);
	
	return 0;
}

/*---------------------------------------------------------------------------------------------*/

void DeleteTcpServer()
{
	close(client_fd);
    close(server_fd);
	return;
}

/*---------------------------------------------------------------------------------------------*/

void TcpDispatcher()
{
	int ret;
	struct timeval tv;
	struct pollfd pfd = {0};

	COMMON_PACKET_T *server_answer = (COMMON_PACKET_T *)server_answer_buf;
	server_answer->packet_id = SERVER_ANSWER;
	server_answer->data_size = 1;
	tv.tv_sec = 3;   
	tv.tv_usec = 0;
	
	ret = CreateTcpServer();
	if(ret != 0){
		printf("[%s], first call CreateTcpServer error, exit..\n", __func__);
		atomic_store(&flag_stop, 1);
		return;
	}
	
	pfd.fd = server_fd;
	pfd.events = POLLIN;
	
	while(!flag_stop)
	{
		ret = poll(&pfd, 1, 1000); 
		if(ret == 0) {
			//printf("[%s], elapsed timeout accept\n", __func__);
			continue;
		}
		
		client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
		if(client_fd < 0) 
		{
			close(server_fd);
			sleep(1);
			if(CreateTcpServer() != 0){
				printf("[%s] ,second call CreateTcpServer error, exit..\n", __func__);
				atomic_store(&flag_stop, 1);
				return;
			}
			continue;
		}
	
		printf("[%s], Server is accept on port %d...\n", __func__, PORT);
		
		ret = setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		if (ret < 0) {
			close(client_fd);
			printf("[%s], setsockopt return %d\n", __func__, ret);
			continue;
		}
		
		while(!flag_stop)
		{
			int rd_size;
			NODE_T *item = FifoPop(&net_fifo);
			if(!item){
				printf("[%s], no free slots... \n", __func__);
				sleep(1);
				continue;
			}
			
			rd_size = read(client_fd, item->ptr_mem, item->item_size);
			if(rd_size < 0){
				close(client_fd);
				printf("[%s], read timeout, connection closed \n", __func__);
				break;
			}
			else if(rd_size == 0)
			{
				close(client_fd);
				printf("[%s], connection closed by client \n", __func__);
				break;
			}
			else{
				COMMON_PACKET_T *client_packet = (COMMON_PACKET_T *)item->ptr_mem;
				uint8_t *ptr_client_data = (uint8_t *)client_packet->ptr_data;
				uint16_t crc = client_packet->packet_id + client_packet->packet_number + client_packet->data_size;
				// server answer
				server_answer->ptr_data[0] = REQUEST_OK;
				
				for(int i = 0; i < client_packet->data_size; i++)
					crc += ptr_client_data[i];
				
				if(client_packet->crc == crc){
					if(client_packet->packet_id == LIGHT_PACKET){
						ret = FifoPush(&light_fifo, item);
					}
					else if(client_packet->packet_id == CHASSIS_PACKET){
						ret = FifoPush(&chassis_fifo, item);
					}
					else if(client_packet->packet_id == CAMERA_XY_PACKET){
						ret = FifoPush(&camera_xy_fifo, item);
					}
					else{
						server_answer->ptr_data[0] |= REQUEST_UNKNOW_PCKT_ID;
					}
				}
				else{
					printf("[%s], wrong crc, received = %x, calculated = %x\n", __func__, client_packet->crc, crc);
					server_answer->ptr_data[0] |= REQUEST_WRONG_CRC;
					ret = FifoPush(&net_fifo, item);
				}
			
				if(ret != 0)
				{
					server_answer->ptr_data[0] |= REQUEST_ERROR_FIFO_RELEASE;
					printf("[%s], can't release net fifo item, stop everything..\n", __func__);
					atomic_store(&flag_stop, 1);
				}

				server_answer->packet_number = client_packet->packet_id;
				server_answer->crc = server_answer->packet_id + server_answer->packet_number + server_answer->data_size + server_answer->ptr_data[0];
				ret = send(client_fd, (void *)&server_answer_buf, sizeof(COMMON_PACKET_T) + server_answer->data_size, 0);
				if(ret == -1){
					printf("[%s], send return -1\n", __func__);
					close(client_fd);
					break;
				}
				//printf("[%s], answer send\n", __func__);
			}
		}
		
	}
	
	DeleteTcpServer();
	
	return;
}

/*---------------------------------------------------------------------------------------------*/
