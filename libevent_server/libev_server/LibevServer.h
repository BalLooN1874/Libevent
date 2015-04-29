#pragma once
#include <functional>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include <list>
#include <event2/buffer.h>
#include <event2/event_struct.h>
#include <event2/event.h>    
#include <event2/listener.h>    
#include <event2/bufferevent.h>    
#include <event2/util.h> 

struct event_base;
struct bufferevent;

class CEventData
{
public:
	typedef std::function<void()> Functor;
	CEventData();
	~CEventData();
	
	event_base* startEventLoop();
	void runEventLoop(const Functor& cbPtr);
	std::thread::id getCurThreadID() const 
	{
		return m_threadID;
	}

private:
	void wakeup();
	static void wakeupCallback(evutil_socket_t, short, void* arg);
	void threadFunc();

private:
	event_base* m_ptrBase;
	bool			   m_exiting;
	std::thread   m_thread;
	std::mutex   m_mutex;
	std::condition_variable  m_cond;
	std::list<Functor>  m_listFunctors;
	evutil_socket_t	m_wakeFd[2];
	evutil_socket_t	m_connFd;
	event				m_ev;
	std::thread::id	m_threadID;
};

class CEventDataPool
{
public:
	CEventDataPool();
	~CEventDataPool();

	void setThreadNum(const int nNum);
	void start();
	CEventData* getNextEventData();
	std::thread::id getCurThreadID() const;
private:
	std::list<CEventData*> m_listEventDataPool;
	int m_nNum;
	int m_nNext;
	CEventData* m_ptrCurEventData;
};

class CLibevServer
{
public:
	typedef std::function<void(bufferevent* bev)> ReadCallback;
	typedef ReadCallback WriteCallback;
	typedef std::function<void(bufferevent* bev, short what)> EventCallback;

	CLibevServer(event_base* base, evutil_socket_t fd);
	CLibevServer(unsigned short port);
	~CLibevServer();

	void runServer();
public:
	//新建连接成功后，会调用该函数
	virtual void ConnectionEvetn(int nMsgType, int nMsgID, void* pData, void* pReserved = nullptr){}
	//读取完数据后，会调用该函数
	virtual void ReadEvent(int nMsgType, int nMsgID, void* pData, void* pReserved = nullptr){}
	//发送完成功后，会调用该函数（因为串包的问题，所以并不是每次发送完数据都会被调用）
	virtual void WriteEvent(int nMsgType, int nMsgID, void* pData, void* pReserved = nullptr){}
	//断开连接（客户自动断开或异常断开）后，会调用该函数
	virtual void CloseEvent(int nMsgType, int nMsgID, void* pData, void* pReserved = nullptr){}
	//发生致命错误（如果创建子线程失败等）后，会调用该函数
	//该函数的默认操作是输出错误提示，终止程序
	virtual void ErrorQuit(const char* str){}
private:
	CLibevServer(const CLibevServer&) = delete;
	CLibevServer& operator=(const CLibevServer&) = delete;
	static void readCallBack(bufferevent* bev, void* data);
	static void writeCallback(bufferevent* bev, void* data);
	static void eventCallback(bufferevent* bev, short what, void* data);
	static void listenCallback(evconnlistener* listener, evutil_socket_t fd, sockaddr* addr, int socklen, void* arg);
	void start();
private:
	ReadCallback m_readCallback;
	WriteCallback m_writeCallback;
	EventCallback m_eventCallback;
	bufferevent*   m_ptrBufferEvent;
	unsigned short m_port;
	evconnlistener* m_pListener;
	CEventDataPool m_pEventPool;
};

