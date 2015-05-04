// libevent_TcpEventServer.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <iostream>
#include "TcpServer.h"
#include <Windows.h>

int _tmain(int argc, _TCHAR* argv[])
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	{
		CTcpServer tcpServer(9995);
		tcpServer.start();
	}

	system("pause");
	WSACleanup();
	return 0;
}

