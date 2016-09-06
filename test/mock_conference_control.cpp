/****************************************************************************************
* Copyright (c) 2015~2015 All Rights Resverved by
*  . 
****************************************************************************************/
/**
* @file 
* @brief 
*
* @author shangrong.rao
*
* @date 
*
* @version 0.1.1
*
* Revision History 
* @if  CR/PR ID Author	   Date		  Major Change @endif
*               shangrong.rao May. 22 2015 create version 0.0.1\n
****************************************************************************************/
/*------------------------------- HEADER FILE INCLUDES ---------------------------------*/
#include <functional>  
#include <condition_variable>  
#include <mutex>  
#include <thread>  

#include "event2/event.h"  
#include "event2/event_struct.h"  
#include "event2/util.h"  
#include "event2/buffer.h"  
#include "event2/listener.h"  
#include "event2/bufferevent.h"  

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>  
#include <string.h>

#include <arpa/inet.h>

#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/config/SourcePrefix.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/RepeatedTest.h>
#include <cppunit/extensions/HelperMacros.h>


#include "test_common.h"
#include "../bridgeserver/ini.hpp"
#include "../bridgeserver/util.h"
#include "../bridgeserver/applog.h"
#include "../bridgeserver/IceConnection.h"
#include "../bridgeserver/IceService.h"
#include "../bridgeserver/TcpServer.h"

#include "mock_conference_control.h"

using namespace std;
/*------------------------------------  DEFINITION ------------------------------------*/
void startCallee(const char* caller_content, int caller_content_len, char* callee_content, int& callee_content_len);

/*----------------------------------- ENUMERATIONS ------------------------------------*/
/*-------------------------------------- CONSTANTS ------------------------------------*/
/*-------------------------- STRUCT/UNION/CLASS DATA TYPES ----------------------------*/
class Callee {
public:
	std::thread*	thread_;
	eice_t			callee_;

	Callee() : thread_(NULL), callee_(NULL)
	{
	}
	~Callee() {}
	void stop() {
		if (thread_ != NULL) {
			thread_->join();
			delete thread_;
		}
	}

	void start(const char* caller_content, int caller_content_len, char* callee_content, int& callee_content_len)
	{
		INI::Parser ini("config.ini"); 
		string iceJsonConfig = ini.top()("GLOBAL")["ice_json_config"];
		
		/*test_expect callee_expect;
		callee_expect.local_is_host_cand = PJ_TRUE;
		callee_expect.local_is_srflx_cand = PJ_TRUE;
		callee_expect.local_is_ms_cand = PJ_FALSE;
		callee_expect.local_is_turn_addrs = PJ_TRUE;
		callee_expect.result = 0;
		callee_expect.is_relay = PJ_FALSE;*/
		
		int ret = eice_new_callee(iceJsonConfig.c_str(), caller_content, caller_content_len, callee_content, &callee_content_len, &callee_);
		LOG_INFO("eice_new_callee return " << ret);
		if (ret){
			LOG_INFO("something wrong with new callee !!!");
			return;
		}

		thread_ = new std::thread(std::bind(&Callee::getResult, this));
	}

	void getResult()
	{
		char  callee_result[8 * 1024];
		int callee_result_len = 0;

		int ret = 0;

		do{
			/*ret = callee_expect.check_content(callee_content);
			if (ret){
			dbg("check callee content fail !!!");
			break;
			}*/

			while (callee_result_len == 0){
				ret = eice_get_nego_result(callee_, callee_result, &callee_result_len);
				if (ret == 0 && callee_result_len > 0){
					LOG_INFO("+++++++++get callee nego result OK, this result will set to android client as its local address");
					LOG_INFO(callee_result);
				}

				usleep(50000);
			}

			/*ret = callee_expect.check_result(callee_result);
			if (ret != 0){
			dbg("callee expect fail !!!");
			break;
			}*/

			
		} while (false);

		if (callee_ != NULL)	eice_free(callee_);
	}
};

class CcsConnection : public IceConnection
{
public:
	CcsConnection(struct bufferevent *bev, CCS* server) : IceConnection(bev), server_(server) {}
	~CcsConnection() {
		callee_.stop();
	}

	virtual void handleIceCommand(const IceCommand &command)
	{
		LOG_INFO("Receive command: " << command.op << ", id: " << command.sessionId
			<< ", content: " << command.content << "; len: " << command.content.size());

		switch (command.op)
		{
		case ICE_OP_LOCAL_INFO:
		{			
			char  callee_content[8 * 1024];
			int callee_content_len = 0;
						
			callee_.start(command.content.c_str(), command.content.size(), callee_content, callee_content_len);

			sendCommand(ICE_OP_START_SESSION, command.sessionId, callee_content, callee_content_len);
		}
		break;
		case ICE_OP_ICE_RESULT:
		{
			// write ice result to a file
			/*const std::set<Connection*>& conns = server_->webrtcServer_->connections();
			if (!conns.empty())		
			{
				char buff[100];
				printf(buff, "sessionId:%d\n", command.sessionId);
				(*conns.begin())->send(buff, strlen(buff));
				std::string data = "data:" + command.content + "\n";
				(*conns.begin())->send(data.c_str(), data.size());			
			}*/
			{
				std::ofstream output;
				output.open("/home/shangrong/iceAddr.json", ios::out | ios::trunc);
				if (output.good()) {
					output << command.content;
				}
				else {
					LOG_DEBUG("can not open file iceAddr.json.");
				}
			}

			// tell sender, 将webrt已经协商好的结果写到配置文件中，js会议服务器之后读出来
			int count = 0;
			while (++count < 30)
			{
				try
				{				
					INI::Parser parser("/home/shangrong/answer.ini");
					std::string answer = parser.top()["webrtc_answer"];
					if (answer.empty()) {
						usleep(1000000);
						LOG_DEBUG("answer.ini is empty. "  << count);
						continue;
					}

					sendCommand(ICE_OP_RELAY_SESSION, command.sessionId, answer.c_str(), answer.size());
					break;
				}
				catch (...)
				{
					LOG_DEBUG("can not open file answer.ini.");
				}
			}
		}
		break;
		default:
			LOG_INFO("Unknowed command.");
			break;
		}
	}

	void sendCommand(ICE_OP op, SessionID id, const char* data, uint32_t dataLen)	
	{
		LOG_INFO("Send command " << op << "; Session id: " << id << "; data len " << dataLen);

		constructCommandBuffer(sendBuffer_, op, id, data, dataLen);		
		
		LOG_DEBUG("Will send data: " << toHexString(sendBuffer_, std::min(16, (int)(ICE_COMMAND_HEADER_LEN + dataLen)), " "));

		if (send(sendBuffer_, dataLen + ICE_COMMAND_HEADER_LEN) != 0) {
			LOG_WARN("Can't send the whole command, " << dataLen + ICE_COMMAND_HEADER_LEN);
		}
	}

private:
	char	sendBuffer_[MAX_COMMAND_LEN];

	CCS*		server_;
	Callee		callee_;
};



class WebrtcConnection : public Connection
{
public:
	WebrtcConnection(struct bufferevent *bev) : Connection(bev) {}
	virtual void handleCommand(const char *data, unsigned int len)
	{
		LOG_DEBUG("receive " << data << "; len " << len);
		//if (strcmp(data, ) == 0) {
			//sendCommand(ICE_OP_RELAY_SESSION, command.sessionId, answer.c_str(), answer.size());
		//}
	}
	
private:
	std::string		recvBuffer_;
};
class WebrtcConnectionBuilder :public ConnectionBuilder {
	virtual Connection* create(bufferevent *bev) { return new WebrtcConnection(bev); }
};


CCS::CCS() : eventBase_(NULL), bridgeClient_(NULL), threadCall_(NULL), start_flag_(0) {}

void CCS::call(const Functor& f)
{
	assert(threadCall_ != NULL);
	threadCall_->threadCall(f);
}

void CCS::timeout_cb(evutil_socket_t fd, short event, void *arg)
{
	assert(arg != NULL);
	CCS* self = (CCS*)arg;
	
	INI::Parser ini("start.ini");
	std::string sflag = ini.top()["start_flag"];
	if (sflag.empty())
		return;

	unsigned flag = atoi(sflag.c_str());
	if (flag != self->start_flag_) {
		self->start_flag_ = flag;
		self->startACaller();
	}
}

void CCS::startConferenceControlServer()
{
	const char* server_ip = "0.0.0.0";
	INI::Parser ini("config.ini");
	unsigned server_port = atoi(ini.top()("GLOBAL")["local_tcp_server_port"].c_str());

	//创建一个event的实例
	eventBase_ = event_base_new();

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;

	struct hostent *remoteHost = gethostbyname(server_ip);
	if (remoteHost == NULL || remoteHost->h_addr_list[0] == NULL)
	{
		inet_aton(server_ip, &server_addr.sin_addr);
	}
	else
	{
		server_addr.sin_addr.s_addr = *(u_long *)remoteHost->h_addr_list[0];
	}
	//inet_addr
	server_addr.sin_port = htons(server_port);

	
	struct bufferevent *client = bufferevent_socket_new(eventBase_, -1, BEV_OPT_CLOSE_ON_FREE);//创建SocketEvent对象
	if (NULL == client)
	{
		LOG_ERROR("bufferevent_socket_new failed, can't connect  IM");
		return;
	}		
	bufferevent_socket_connect(client, (struct sockaddr *)&server_addr, sizeof(server_addr));
	bridgeClient_ = new CcsConnection(client, this);
	

	/// create a timer, 为测试方便，检查文件，看看是否需要启动新的测试		
	struct timeval tv;
	event_assign(&timeout_, eventBase_, -1, EV_PERSIST, timeout_cb, this);
	evutil_timerclear(&tv);
	tv.tv_sec = 2;
	event_add(&timeout_, &tv);


	threadCall_ = new EventThreadCall(eventBase_);
	
	webrtcServer_.reset(new TcpServer(6868, eventBase_, new WebrtcConnectionBuilder()));

	LOG_DEBUG("start ccs, wait for data.");
	event_base_dispatch(eventBase_);	
}

CCS::~CCS()
{	
	delete threadCall_;
	delete bridgeClient_;	
}
void CCS::start()
{
	/// 有多线程问题，暂时不管了
	if (eventBase_ != NULL)
		return;
	thread_ = new std::thread(std::bind(&CCS::startConferenceControlServer, this));
}

void CCS::stop_in()
{
	struct timeval delay = { 0, 0 };
	event_base_loopexit(eventBase_, &delay);
}

void CCS::stop()
{	
	// should in the same thread
	call(std::bind(&CCS::stop_in, this));

	// should in other thread
	thread_->join();
	delete thread_;
}



void CCS::startACaller()
{
	bridgeClient_->sendCommand(ICE_OP_PREPARE_SESSION, 0, NULL, 0);
}


int  g_count = 0;
void increaseCount()
{
	++g_count;
	LOG_INFO("Current count is: " << g_count);
}

#ifdef MAIN
#include "../bridgeserver/AppServer.h"
AppServer g_app;
int main(int argc, char ** argv) {
	CCS ccs;
	ccs.startConferenceControlServer();
	return 0;
}
#endif

/*------------------------------------- GLOBAL DATA -----------------------------------*/
/*----------------------------- LOCAL FUNCTION PROTOTYPES -----------------------------*/
/*-------------------------------- FUNCTION DEFINITION --------------------------------*/
