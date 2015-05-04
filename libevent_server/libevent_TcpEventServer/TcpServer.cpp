#include "TcpServer.h"
#include <iostream>

//////////////////////////////////////////////////////////////////////////
//EventLoopThread class
EventLoopThread::EventLoopThread() : base_(nullptr)
,exiting_(false)
,thread_(std::bind(&EventLoopThread::threadFunc, this))
,threadID_(std::this_thread::get_id())
{
}
EventLoopThread::~EventLoopThread()
{
	exiting_ = true;
	event_base_loopexit(base_, nullptr);
	thread_.join();
}

struct  event_base* EventLoopThread::startLoop()
{
	{
		std::unique_lock<std::mutex> ul(mutex_);
		while (nullptr == base_)
		{
			cond_.wait(ul);
		}
	}
	return base_;
}

void EventLoopThread::runInLoop(const Functor& cb)
{
	{
		std::unique_lock<std::mutex> ul(mutex_);
		functors_.push_back(cb);
	}
	wakeup();
}

void EventLoopThread::wakeup()
{
	char c = 'w';
	::send(wakeFd_[0], &c, 1, 0);
}

void EventLoopThread::WakeupCallback(evutil_socket_t, short, void* arg)
{
	EventLoopThread* p = reinterpret_cast<EventLoopThread*>(arg);
	if (p)
	{
		char buf[16] = { 0 };
		::recv(p->wakeFd_[1], buf, sizeof(buf), 0);
		std::vector<Functor> functors;
		{
			std::lock_guard<std::mutex> lock(p->mutex_);
			functors.swap(p->functors_);
		}
		for (auto& f : functors)
		{
			f();
		}
	}
}

void EventLoopThread::threadFunc()
{
	struct event_base* base = event_base_new();
	int iret = evutil_socketpair(AF_INET, SOCK_STREAM, 0, wakeFd_);
	std::cout << "socket pair ret:" << iret << std::endl;
	std::cout << "thread id:" << std::this_thread::get_id() << std::endl;
	iret = event_assign(&ev_, base, wakeFd_[1], EV_READ | EV_PERSIST, WakeupCallback, this);
	if (0 != iret)
	{
		std::cout << "event assign error" << iret << std::endl;
	}

	iret = event_add(&ev_, nullptr);
	if (0 != iret)
	{
		std::cout << "event add error:" << iret << std::endl;
	}
	{
		std::unique_lock<std::mutex> ul(mutex_);
		base_ = base;
		cond_.notify_one();
	}
	event_base_dispatch(base);
	std::cout << "event loop exiting...\n";
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//EventLoopThreadPool class
EventLoopThreadPool::EventLoopThreadPool():num_(1), next_(0)
{

}

EventLoopThreadPool::~EventLoopThreadPool()
{
	for (auto loop : loops_)
	{
		delete loop;
	}
}

void EventLoopThreadPool::setThreadNum(int num)
{
	num_ = num;
}

void EventLoopThreadPool::start()
{
	for (int i = 0; i < num_; ++i)
	{
		EventLoopThread* loop = new EventLoopThread();
		loops_.push_back(loop);
	}
}

EventLoopThread* EventLoopThreadPool::getNextLoop()
{
	EventLoopThread* pCurrent = loops_[next_++];
	next_ %= num_;
	return pCurrent;
}

std::thread::id EventLoopThreadPool::currentThreadID() const 
{
	if (next_ - 1 >=0)
	{
		return loops_[next_ - 1]->getThreadID();
	}
	else
	{
		return std::thread::id();
	}
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//Channel
Channel::Channel(struct event_base* base, evutil_socket_t fd) :
bufferEvent_(bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE))
{

}

Channel::~Channel()
{

}

void Channel::start()
{
	bufferevent_setcb(bufferEvent_, readCallback, writeCallback, eventCallback, this);
	int iret = bufferevent_enable(bufferEvent_, EV_READ | EV_WRITE);
	if (0 != iret)
	{
		std::cout << "bufferent enable error" << std::endl;
	}
}
void Channel::readCallback(struct bufferevent* bev, void* data)
{
	char msg[4096] = { 0 };
	fprintf(stdout, "readCallback..., threadID:%d\n", ::GetCurrentThreadId());
	struct evbuffer* input = bufferevent_get_input(bev);
	size_t len = evbuffer_get_length(input);
	//读取数据
	len =  bufferevent_read(bev, msg, 4096);
	echo_context* ptrData = (echo_context*)msg;
	std::cout << "server read the data: " << ptrData->echo_contents << std::endl;
	evbuffer_drain(input, len);//将读取到的数据移除掉
	fprintf(stdout, "drain, len:%d\n", len);

 	char* reply = "i has read you data";
	echo_context pp;
	memset(&pp, 0, sizeof(echo_context));
	memcpy(pp.echo_contents, "i has read you data", 80);
	bufferevent_write(bev, (char*)&pp, sizeof(echo_context));
}

void Channel::writeCallback(struct bufferevent* bev, void* data)
{
	fprintf(stdout, "writeCallback..., threadID:%d\n", ::GetCurrentThreadId());
	struct evbuffer* output = bufferevent_get_output(bev);

	size_t len = evbuffer_get_length(output);
// 	int buflen = strlen("i has read you data");
// 	printf("output_len: %d\n", len);
}

void Channel::eventCallback(struct bufferevent *bev, short what, void *data)
{
	if (!data) 
	{
		return;
	}
	Channel *p = (Channel*)data;
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
//////////////////////////////////////////////////////////////////////////
CTcpServer::CTcpServer(unsigned short port):
port_(port)
{
}


CTcpServer::~CTcpServer()
{
}

void CTcpServer::start()
{
	eventPool_.setThreadNum(2);
	eventPool_.start();
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(9995);

	struct event_base* base = event_base_new();
	listener_ = evconnlistener_new_bind(base, listenCallback, this, 
															LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
															-1, (sockaddr*)&sin, sizeof(sin));

	event_base_dispatch(base);
	evconnlistener_free(listener_);
	event_base_free(base);
}

void CTcpServer::listenCallback(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr* addr, int socklen, void* data)
{
	if (!data)
	{
		return;
	}
	fprintf(stdout, "new connection is comming...\n");
	CTcpServer* p = reinterpret_cast<CTcpServer*>(data);
	EventLoopThread* loop = p->eventPool_.getNextLoop();
	Channel* channel = new Channel(loop->startLoop(), fd);
	loop->runInLoop(std::bind(&Channel::start, channel));
	fprintf(stdout, "currentThreadID:%d\n", ::GetCurrentThreadId());
}