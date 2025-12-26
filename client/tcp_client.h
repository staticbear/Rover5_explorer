#ifndef _TCP_CLIENT_H_
#define _TCP_CLIENT_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <stdatomic.h>

int  check_ip_addr(const char *ip);
int  Tcp_Connect(char *server_ip, int port);
void Tcp_Close();
void TcpRecvDispatcher();
void TcpBatteryDispatcher();

#endif