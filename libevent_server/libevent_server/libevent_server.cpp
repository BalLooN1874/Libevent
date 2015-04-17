// libevent_server.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <io.h>    
#include <WinSock2.h>    
#include <stdlib.h>
#include<stdio.h>    
#include<string.h>    
#include <iostream>
#include <event2/event.h>    
#include <event2/listener.h>    
#include <event2/bufferevent.h>    
#include <thread>

using namespace std;

void listener_cb(evconnlistener* listener, evutil_socket_t fd, struct sockaddr* sock, int socklen, void* arg);
void sock_read_cb(bufferevent* bev, void* arg);
void sock_event_cb(bufferevent* bev, short events, void* arg);

int _tmain(int argc, _TCHAR* argv[])
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	wVersionRequested = MAKEWORD(1, 1);

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		return 0;
	}


	if (LOBYTE(wsaData.wVersion) != 1 ||
		HIBYTE(wsaData.wVersion) != 1) {
		WSACleanup();
		return 0;
	}


	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(9995);

	event_base* base = event_base_new();
	evconnlistener* listener = evconnlistener_new_bind(base, listener_cb, base, \
																					LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, \
																					10, (sockaddr*)&sin, sizeof(sockaddr_in));
	int a = event_base_dispatch(base);
	evconnlistener_free(listener);
	event_base_free(base);
	return 0;
}

//一个新客服端连接服务器
//当此函数被调用时， libevent已经做好了accept这个客户端，该客服端文件描述符为fd
void listener_cb(evconnlistener* listener, evutil_socket_t fd, struct sockaddr* sock, int socklen, void* arg)
{
	cout << "accept a client : " << fd << endl;
	event_base* base = (event_base*)arg;

	//为新的客户端分配一个bufferevent
	bufferevent* bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(bev, sock_read_cb, nullptr, sock_event_cb, nullptr);
	bufferevent_enable(bev, EV_READ | EV_PERSIST);
}

void sock_read_cb(bufferevent* bev, void* arg)
{
	char msg[4096] = { 0 };
	size_t len = bufferevent_read(bev, msg, sizeof(msg)-1);
	msg[len] = '\0';
	cout << "server read the data: " << msg << endl;
	char* reply = "i has read you data";
	bufferevent_write(bev, reply, strlen(reply));
}

void sock_event_cb(bufferevent* bev, short events, void* arg)
{
	if (events & BEV_EVENT_EOF)
	{
		cout << "connection closed\n";
	}
	else if (events & BEV_EVENT_ERROR)
	{
		cout << "some other error\n";
	}

	//自动lclose套接字和free读写缓冲区
	bufferevent_free(bev);
}


//42,hello,2.3,a
//hello,2.3,a
//2.3,a
//a
//1,3,6
