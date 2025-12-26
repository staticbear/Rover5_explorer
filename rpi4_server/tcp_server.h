#ifndef _TCP_SERVER_H_
#define _TCP_SERVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <stdatomic.h>

#define RAW_BUFFER_SIZE 1024

int CreateTcpServer();
void TcpRecvDispatcher();
void DeleteTcpServer();
void TcpSendDispatcher();

#endif