// libev_server.cpp : �������̨Ӧ�ó������ڵ㡣
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

