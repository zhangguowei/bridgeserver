/****************************************************************************************
* Copyright (c) 2015~2015 All Rights Resverved by
*  
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


#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/config/SourcePrefix.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/RepeatedTest.h>
#include <cppunit/extensions/HelperMacros.h>

#include "test_common.h"
#include "../bridgeserver/applog.h"
#include "../bridgeserver/AppServer.h"
#include "../bridgeserver/libevent_thread_call.h"
#include "../bridgeserver/ForwardService.h"

#include "mock_conference_control.h"

using namespace std;

class Udp {
public:
	bool create(unsigned port) {
		fd = createUdpSocket(port);
		if (fd <= 0){
			LOG_WARN("error local fd from ice result.");
			return false;
		}

		evutil_make_socket_nonblocking(fd);
		
		event* ev_udp = event_new(g_app.eventBase(), fd, EV_READ | EV_PERSIST, udpCallback, this);
		if (!ev_udp) {
			LOG_ERROR("Cann't new event for udp transport.");
			return false;
		}

		event_add(ev_udp, NULL);

		return true;
	}

	static void udpCallback(evutil_socket_t fd, short what, void *arg)
	{
		if (!(what&EV_READ)) return;
		assert(arg != NULL);
		Udp* self = (Udp*)arg;
		struct sockaddr_in tempadd;
		ev_socklen_t addrLen = sizeof(tempadd);
		
		char buffer[1024];
		while (true)
		{
			int dataLen = recvfrom(fd, buffer, MAX_UDP_PACKET_LEN, 0, (struct sockaddr*)&tempadd, &addrLen);
			if (dataLen <= 0){
				break;
			}			
			LOG_DEBUG("receive data len: " << dataLen << "; from " << ntohs(tempadd.sin_port) << "; my fd " << fd << "; peer port: " << ntohs(self->peerAddr.sin_port));

			usleep(300000);

			int sendLen = sendto(fd, buffer, dataLen, 0, (struct sockaddr*)&tempadd, addrLen);			
			LOG_DEBUG("send it back");
		}		
	}
		
	int				fd;
	sockaddr_in		peerAddr;
};


/*------------------------------------  DEFINITION ------------------------------------*/
/*----------------------------------- ENUMERATIONS ------------------------------------*/
/*-------------------------------------- CONSTANTS ------------------------------------*/
/*-------------------------- STRUCT/UNION/CLASS DATA TYPES ----------------------------*/

class TestForward : public CPPUNIT_NS::TestFixture {
	CPPUNIT_TEST_SUITE( TestForward );
	CPPUNIT_TEST( test_ice );
	CPPUNIT_TEST(test_all);
	CPPUNIT_TEST(test_filter);
	CPPUNIT_TEST_SUITE_END();

public:
	void setUp()
	{
		printf("run to TestForward.\n");		
	}
	void tearDown()
	{	
		printf("quit to TestForward.\n");
	}
	
public:
	REPEAREDTIMES_PROXY(TestForward);

	void test_ice() {		
	}

	
	void test_all()
	{
		return;

		ForwardService&  forward = *(new ForwardService());

		AddrPair addr_pair;
		addr_pair.local_fd = 0;
		addr_pair.local_ip = "172.17.3.7";
		addr_pair.local_port = 12245;
		addr_pair.remote_ip = "172.17.3.7";
		addr_pair.remote_port = 13245;


		sockaddr_in webrtcAddr;
		memset(&webrtcAddr, 0, sizeof(struct sockaddr_in));
		webrtcAddr.sin_family = AF_INET;
		webrtcAddr.sin_port = htons(23245);
		webrtcAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		
		forward.startForward(MEDIA_AUDIO, addr_pair, webrtcAddr);

		ForwardService::Address local;
		local.ip = addr_pair.local_ip;
		local.port = addr_pair.local_port;
		CPPUNIT_ASSERT(forward.forwards_.find(local) != forward.forwards_.end());
		

		sockaddr_in forwardAddr;
		memset(&forwardAddr, 0, sizeof(struct sockaddr_in));
		forwardAddr.sin_family = AF_INET;
		forwardAddr.sin_port = htons(12245);
		forwardAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		Udp& u1 = *(new Udp());
		Udp& u2 = *(new Udp());
		u1.peerAddr = u2.peerAddr = forwardAddr;
		u1.create(13245);
		u2.create(23245);

		char buffer[] = "hello";
		sendto(u1.fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&forwardAddr, sizeof(forwardAddr));
	}

	void test_filter() {
		rtp_hdr curRtpHeader;
		curRtpHeader.ts = 0x1000;
		uint32_t tempi = curRtpHeader.timeStamp() * 3;
		curRtpHeader.setTimeStamp(tempi);
		CPPUNIT_ASSERT(curRtpHeader.ts == 0x3000);
		
		
		/// now modify timestamp
		char data[13] = "123456789012";
		
		rtp_hdr& rtpHeader = *(rtp_hdr*)&data;
		rtpHeader.ts = 0x123456;
		rtpHeader.setTimeStamp(rtpHeader.timeStamp() * 3);
		//CPPUNIT_ASSERT(rtpHeader.ts == 0x123456 * 3);
		
		rtpHeader.setTimeStamp(101);
		CPPUNIT_ASSERT(rtpHeader.timeStamp() == 101);

		uint32_t& st = *(uint32_t*)&data[4];
		st = 0x123412;
		AudioRtpFilter af;
		af.filter(data, 12, 3);
		LOG_DEBUG("after ratio 3, st is " << st);
		CPPUNIT_ASSERT(st == 0x123412 * 3);

		st = 0x123456;
		af.filter(data, 12, -3);
		LOG_DEBUG("after ratio -3, st is " << st);
		//CPPUNIT_ASSERT(st == 0x123456 / 3);
	}

};


/*------------------------------------- GLOBAL DATA -----------------------------------*/
/*----------------------------- LOCAL FUNCTION PROTOTYPES -----------------------------*/
/*-------------------------------- FUNCTION DEFINITION --------------------------------*/
void start_test_forward(CppUnit::TextUi::TestRunner& runner) {
	runner.addTest(TestForward::suite());
}
//CPPUNIT_TEST_SUITE_REGISTRATION( REPEAREDTIMES_REG(TestForward, 1) );
