#pragma once
#include <functional>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include <event2/buffer.h>
#include <event2/event_struct.h>
#include <event2/event.h>    
#include <event2/listener.h>    
#include <event2/bufferevent.h>    
#include <event2/util.h> 
struct event_base;
struct bufferevent;

class EventLoopThread
{
public:
	typedef std::function<void()> Functor;
	EventLoopThread();
	~EventLoopThread();

	struct event_base* startLoop();
	void   runInLoop(const Functor& cb);
	std::thread::id getThreadID()
	{
		return threadID_;
	}
private:
	void wakeup();
	static void WakeupCallback(evutil_socket_t, short, void* arg);
	void threadFunc();
private:
	struct event_base* base_;
	bool exiting_;
	std::thread thread_;
	std::mutex mutex_;
	std::condition_variable cond_;
	std::vector<Functor> functors_;
	evutil_socket_t wakeFd_[2];
	evutil_socket_t connFd_;
	struct event ev_;
	std::thread::id threadID_;
};

class EventLoopThreadPool
{
public:
	EventLoopThreadPool();
	~EventLoopThreadPool();

	void setThreadNum(const int num);
	void start();
	EventLoopThread* getNextLoop();
	std::thread::id currentThreadID() const;
private:
	std::vector<EventLoopThread*> loops_;
	int num_;
	int next_;
};

class Channel
{
public:
	typedef std::function<void(struct bufferevent* bev)> ReadCallback;
	typedef ReadCallback WriteCallback;
	typedef std::function<void(struct bufferevent* bev, short what)> EventCallback;

	Channel(struct event_base* base, evutil_socket_t fd);
	~Channel();

	void start();
private:
	Channel(const Channel&) = delete;
	Channel& operator=(const Channel&) = delete;
	static void readCallback(struct bufferevent* bev, void* data);
	static void writeCallback(struct bufferevent* bev, void* data);
	static void eventCallback(struct bufferevent* bev, short what, void* data);
private:
	ReadCallback readCallback_;
	WriteCallback writeCallback_;
	EventCallback eventCallback_;
	struct bufferevent* bufferEvent_;
};
class CTcpServer
{
public:
	CTcpServer(unsigned short port);
	~CTcpServer();
	void start();

private:
	CTcpServer(const CTcpServer&) = delete;
	CTcpServer& operator=(const CTcpServer&) = delete;
	static void listenCallback(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr* addr, int socklen, void* data);
private:
	unsigned short port_;
	struct evconnlistener* listener_;
	EventLoopThreadPool eventPool_;
};

