#ifndef _TCP_SERVER_H_
#define _TCP_SERVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <stdatomic.h>

#define PORT 8080

#define RAW_BUFFER_SIZE 1024

int CreateTcpServer();
void TcpDispatcher();
void DeleteTcpServer();

//void Test();

#endif