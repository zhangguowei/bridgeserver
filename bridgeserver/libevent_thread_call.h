#pragma once

#include "event2/event.h"  
#include "event2/event_struct.h"  
#include "event2/util.h"

#include <assert.h>

#include "applog.h"


#include <functional>
#include <mutex>
#include <queue>

typedef std::function<void()> Functor;    // 如果支持c++11，就是typedef stl::function<void()> Functor;  

static const int FUNCTION_PTR_LEN = sizeof(Functor *);

#if 0
class EventThreadCall_ {
public:
	EventThreadCall_(struct event_base *base)
		: curLen_(0)
	{
		int iret = evutil_socketpair(AF_UNIX, SOCK_DGRAM, 0, wakeFd_);
		evutil_make_socket_nonblocking(wakeFd_[1]);

		std::cout << "socket pair ret:" << iret << std::endl;
		iret = event_assign(&ev_, base, wakeFd_[1], EV_READ | EV_PERSIST, wakeupCallback, this);
		if (0 != iret) {
			std::cout << "event assign error" << iret << std::endl;
		}
		iret = event_add(&ev_, NULL);
		if (0 != iret) {
			std::cout << "event add error" << iret << std::endl;
		}
	}

	// 接管外部提供的函数对象; 
	void threadCall(const Functor& f) const
	{	
		Functor* pf = new Functor(f);
		//int*  pf = new int(1000);
		LOG_INFO("Will call function: " << (uint64_t)pf);
		::send(wakeFd_[0], (char*)&pf, FUNCTION_PTR_LEN, 0);
	}
	
private:
	static void wakeupCallback(evutil_socket_t fd, short, void *arg)
	{
		typedef char* CharPtr;

		EventThreadCall_ *p = (EventThreadCall_*)arg;
		if (p) {			
			int recvLen = 0;	
			int& curLen = p->curLen_;
			const CharPtr& buff = p->buffer_;
			while ((recvLen = ::recv(p->wakeFd_[1], &buff[curLen], FUNCTION_PTR_LEN - curLen, 0)) != -1) {
				LOG_DEBUG("Receive len: " << recvLen << "curLen is " << curLen);
				curLen += recvLen;
				assert(curLen <= FUNCTION_PTR_LEN);
				if (curLen == FUNCTION_PTR_LEN) {					
					LOG_INFO("Received, and will call function: " << (uint64_t)*(Functor**)buff);
					(**(Functor**)buff)();
					delete *(Functor**)buff;

					curLen = 0;
				}
			}			
		}
	}

private:
	struct event    ev_;
	evutil_socket_t wakeFd_[2];

	char			buffer_[FUNCTION_PTR_LEN];
	int				curLen_;
};
#endif

class EventThreadCall {
public:
	EventThreadCall(struct event_base *base) 
	{
		assert(base != NULL);

		int iret = evutil_socketpair(AF_UNIX, SOCK_STREAM, 0, wakeFd_);
		evutil_make_socket_nonblocking(wakeFd_[0]);
		evutil_make_socket_nonblocking(wakeFd_[1]);

		std::cout << "socket pair ret:" << iret << std::endl;
		iret = event_assign(&ev_, base, wakeFd_[1], EV_READ | EV_PERSIST, wakeupCallback, this);
		if (0 != iret) {
			std::cout << "event assign error" << iret << std::endl;
		}
		iret = event_add(&ev_, NULL);
		if (0 != iret) {
			std::cout << "event add error" << iret << std::endl;
		}
	}

	// 接管外部提供的函数对象; 
	void threadCall(const Functor& f)
	{
		{
			std::unique_lock<std::mutex> ul(functionsLock_);
			functions_.push(f);
		}

		static const char singalData[FUNCTION_PTR_LEN] = "singal";
		::send(wakeFd_[0], singalData, FUNCTION_PTR_LEN, 0);
	}

private:
	static void wakeupCallback(evutil_socket_t fd, short, void *arg)
	{
		assert(arg != NULL);
		EventThreadCall *self = (EventThreadCall*)arg;
		char buffer[FUNCTION_PTR_LEN];
		int recvLen = 0;
		if ((recvLen = ::recv(self->wakeFd_[1], buffer, FUNCTION_PTR_LEN, 0)) != -1) {
			//LOG_DEBUG("Receive len: " << recvLen);
			std::unique_lock<std::mutex> ul(self->functionsLock_);
			while (!self->functions_.empty())
			{
				//LOG_DEBUG("call functon.");
				self->functions_.front()();
				self->functions_.pop();
			}
		}
		else {
			//LOG_DEBUG("recv failed.");
		}
	}

private:
	struct event		ev_;
	evutil_socket_t		wakeFd_[2];
	
	std::queue<Functor>	functions_;
	std::mutex			functionsLock_;
};
