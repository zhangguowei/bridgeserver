#include <functional>

#include <event2/event.h>
#include <event2/bufferevent.h>

#include <map>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>

#include <signal.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../eice/src/eice.h"

#include "applog.h"

#include "IceService.h"
#include "TcpServer.h"
#include "ForwardService.h"
#include "AppServer.h"

void log_eice(int level, const char *data, int len)
{
	if (len <= 0)		return;
	
	assert(data != NULL);
	
	switch (level)
	{
	case 1:
		LOG_ERROR(data);
		break;
	case 2:
		LOG_WARN(data);
		break;
	case 3:
		LOG_DEBUG(data);
		break;
	default:
		LOG_DEBUG(data);
		break;
	}	
}

void AppServer::ev_signal_cb(evutil_socket_t sig, short events, void *user_data)
{
	AppServer* self = (AppServer*)user_data;

	long seconds = 0;
	struct timeval delay = { seconds, 0 };
		
	LOG_INFO("stopping...");

	event_base_loopexit(self->eventBase_, &delay);
}

AppServer::AppServer()
{
	LOG_INFO("Start.");

	iniFile_.reset(new IniFile("config.ini"));

	eice_init();

	eice_set_log_func(log_eice);
}

void get_nego_result_func_callback(eice_t obj)
{
	g_app.call(std::bind(&IceService::getNegoResult, g_app.iceService(), obj));
}

void AppServer::start()
{		
	int port = atoi(iniFile_->top()("GLOBAL")["local_tcp_server_port"].c_str());
	eice_set_get_nego_result_func(get_nego_result_func_callback);
		
	//创建一个event的实例
	eventBase_ = event_base_new();
	
	iceServer_.reset(new TcpServer(port, eventBase_, new IceConnectionBuilder()));
	iceService_.reset(new IceService());
	mediaForwardService_.reset(new ForwardService());

	eventThreadCall_.reset(new EventThreadCall(eventBase_));

	struct event *	ev_signal = evsignal_new(eventBase_, SIGINT, ev_signal_cb, (void *)this);
	event_add(ev_signal, NULL);
	
	event_base_dispatch(eventBase_);

	if (ev_signal)		event_free(ev_signal);
	if (eventBase_)		event_base_free(eventBase_);
}

AppServer::~AppServer()
{
	iceService_.reset();
	eice_exit();
}

void AppServer::stop()
{
	struct timeval delay = { 0, 0 };
	event_base_loopexit(eventBase_, &delay);	
}

void AppServer::call(const Functor& f)
{		
	eventThreadCall_->threadCall(f);
}
