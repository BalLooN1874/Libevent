// libev_server.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "LibevServer.h"

int _tmain(int argc, _TCHAR* argv[])
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	{
		CLibevServer tcpServer(9995);
		tcpServer.runServer();
	}

	system("pause");
	WSACleanup();
	return 0;
}

