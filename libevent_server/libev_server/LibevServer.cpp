#include "LibevServer.h"
#include <iostream>
#include <stdio.h>
using namespace std;

//////////////////////////////////////////////////////////////////////////
//CEventData
CEventData::CEventData() 
 :m_ptrBase(nullptr)
 ,m_exiting(false)
 ,m_thread(std::bind(&CEventData::threadFunc, this))
 ,m_threadID(m_thread.get_id())
{
	
}
CEventData::~CEventData()
{
	m_exiting = true;
	event_base_loopexit(m_ptrBase, nullptr);
	m_thread.join();
}

event_base* CEventData::startEventLoop()
{
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		while (nullptr == m_ptrBase)
		{
			m_cond.wait(lock);
		}
	}
	return m_ptrBase;
}

void CEventData::runEventLoop(const Functor& cbPtr)
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_listFunctors.push_back(cbPtr);
	}
	wakeup();
}

void CEventData::wakeup()
{
	char c = 'w';
	::send(m_wakeFd[0], &c, 1, 0);
}

void CEventData::wakeupCallback(evutil_socket_t, short, void* arg)
{
	CEventData* p = reinterpret_cast<CEventData*>(arg);
	if (p)
	{
		char buf[8] = { 0 };
		::recv(p->m_wakeFd[1], buf, sizeof(buf), 0);

		std::list<Functor> functors;
		{
			std::lock_guard<std::mutex> lock(p->m_mutex);
			functors.swap(p->m_listFunctors);
		}
		for (auto& funcPtr : functors)
		{
			funcPtr();
		}
	}
}

void CEventData::threadFunc()
{
	event_base* base = event_base_new();
	int nRet = evutil_socketpair(AF_INET, SOCK_STREAM, 0, m_wakeFd);
	cout << "socket pair ret:" << nRet << endl;
	 
	nRet = event_assign(&m_ev, base, m_wakeFd[1], EV_READ | EV_PERSIST, wakeupCallback, this);
	if (0 != nRet)
	{
		cout << "event assign error:" << nRet << endl;
	}

	nRet = event_add(&m_ev, nullptr);
	if (0 != nRet)
	{
		cout << "event add error:" << nRet << endl;
	}

	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_ptrBase = base;
		m_cond.notify_one();
	}

	event_base_dispatch(base);
	cout << "event loop exiting.....\n";
}
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//CEventDataPool
CEventDataPool::CEventDataPool()
:m_nNum(1)
,m_nNext(0)
,m_ptrCurEventData(nullptr)
{

}

CEventDataPool::~CEventDataPool()
{
	for (auto loop : m_listEventDataPool)
	{
		if (loop)
		{
			delete loop;
			loop = nullptr;
		}
	}
}

void CEventDataPool::setThreadNum(int num)
{
	m_nNum = num;
}

void CEventDataPool::start()
{
	for (int i = 0; i < m_nNum; ++i)
	{
		CEventData* pEventData = new CEventData();
		m_listEventDataPool.push_back(pEventData);
	}
}

CEventData* CEventDataPool::getNextEventData()
{
	CEventData* ptr = nullptr;
	if (!m_listEventDataPool.empty())
	{
		ptr = m_listEventDataPool.front();
		m_ptrCurEventData = ptr;
		m_listEventDataPool.pop_front();
	}
	return ptr;
}
std::thread::id CEventDataPool::getCurThreadID() const 
{
	if (m_ptrCurEventData)
	{
		return m_ptrCurEventData->getCurThreadID();
	}
	else
	{
		return std::this_thread::get_id();
	}
}
//////////////////////////////////////////////////////////////////////////


CLibevServer::CLibevServer(event_base* base, evutil_socket_t fd)
: m_ptrBufferEvent(bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE))
{
	int a = 0;
}
CLibevServer::CLibevServer(unsigned short port)
: m_port(port)
{
}

CLibevServer::~CLibevServer()
{
	//需要处理
}

void CLibevServer::start()
{
	bufferevent_setcb(m_ptrBufferEvent, readCallBack, writeCallback, eventCallback, this);
	int nRet = bufferevent_enable(m_ptrBufferEvent, EV_READ | EV_WRITE);
	if (0 != nRet)
	{
		cout << "bufferevnet enable error:" << nRet << endl;
	}
}

void CLibevServer::readCallBack(bufferevent* bev, void* data)
{
	char msgBuf[4096] = { 0 };
	fprintf(stdout, "ReadCallback..... threadID:%d\n", ::GetCurrentThreadId());
	evbuffer* input = bufferevent_get_input(bev);
	size_t len = evbuffer_get_length(input);
	//读取数据
	len = bufferevent_read(bev, msgBuf, 4096);
	//这里就是接收到消息后，然后转给taskcenter处理。
	//如果是自定义的结构体数据，强转一下就好了。
	std::cout << "server recv data: " << msgBuf << std::endl;
	//将读取到的数据移除掉
	evbuffer_drain(input, len);
	fprintf(stdout, "drain, len:%d\n", len);

	//发送数据到客户端
	//这里操作是等着到时候处理完毕请求了，然后调用bufferevent_write
	char* reply = "i has read you data";
	bufferevent_write(bev, reply, strlen(reply));
}

void CLibevServer::writeCallback(bufferevent* bev, void* data)
{
	fprintf(stdout, "WriteCallback..., threadID:%d\n", ::GetCurrentThreadId());
	struct evbuffer *output = bufferevent_get_output(bev);
}

void CLibevServer::eventCallback(struct bufferevent *bev, short what, void *data)
{
	if (!data)
	{
		return;
	}
	CLibevServer *p = reinterpret_cast<CLibevServer*>(data);

	fprintf(stdout, "EventCallback...\n");
	if (what & BEV_EVENT_READING)
	{
		fprintf(stdout, "BEV_EVENT_READING...\n");
	}
	if (what & BEV_EVENT_WRITING) 
	{
		fprintf(stdout, "BEV_EVENT_WRITING...\n");
	}
	if (what & BEV_EVENT_EOF) 
	{
		fprintf(stdout, "BEV_EVENT_EOF...\n");
	}
	if (what & BEV_EVENT_ERROR)
	{
		fprintf(stdout, "BEV_EVENT_ERROR...\n");
		bufferevent_free(bev);
	}
	if (what & BEV_EVENT_TIMEOUT)
	{
		fprintf(stdout, "BEV_EVENT_TIMEOUT...\n");
	}
	if (what & BEV_EVENT_CONNECTED) 
	{
		fprintf(stdout, "BEV_EVENT_CONNECTED...\n");
	}
}
#define  DEF_THREAD_NUM 1
void CLibevServer::runServer()
{
	m_pEventPool.setThreadNum(DEF_THREAD_NUM);
	m_pEventPool.start();

	sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.S_un.S_addr = htonl(0);
	sin.sin_port = htons(m_port);

	event_base* base = event_base_new();
	m_pListener = evconnlistener_new_bind(base, listenCallback, this, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1, (sockaddr*)&sin, sizeof(sockaddr));
	if (m_pListener)
	{
		event_base_dispatch(base);
	}
	else
	{
		evconnlistener_free(m_pListener);
		event_base_free(base);
	}
}

void CLibevServer::listenCallback(evconnlistener* listener, evutil_socket_t fd, sockaddr* addr, int socklen, void* arg)
{
	if (!arg)
	{
		return;
	}
	fprintf(stdout, "new connection is comming...\n");
	CLibevServer* p = reinterpret_cast<CLibevServer*>(arg);
	CEventData* pEventPool = p->m_pEventPool.getNextEventData();
	CLibevServer* pServer = new CLibevServer(pEventPool->startEventLoop(), fd);
	pEventPool->runEventLoop(std::bind(&CLibevServer::start, pServer));
	fprintf(stdout, "listenCallback function.... currentThreadID:%d\n", pEventPool->getCurThreadID());
}
#if 0
//新建连接成功后，会调用该函数
void CLibevServer::ConnectionEvetn(int nMsgType, int nMsgID, void* pData, void* pReserved /*= nullptr*/)
{

}
//读取完数据后，会调用该函数
void CLibevServer::ReadEvent(int nMsgType, int nMsgID, void* pData, void* pReserved /*= nullptr*/)
{

}
//发送完成功后，会调用该函数（因为串包的问题，所以并不是每次发送完数据都会被调用）
void CLibevServer::WriteEvent(int nMsgType, int nMsgID, void* pData, void* pReserved/* = nullptr*/)
{

}
//断开连接（客户自动断开或异常断开）后，会调用该函数
void CLibevServer::CloseEvent(int nMsgType, int nMsgID, void* pData, void* pReserved /*= nullptr*/)
{

}
//发生致命错误（如果创建子线程失败等）后，会调用该函数
//该函数的默认操作是输出错误提示，终止程序
void CLibevServer::ErrorQuit(const char* str)
{

}
#endif