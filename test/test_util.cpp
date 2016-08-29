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
#include <condition_variable>  
#include <mutex>  
#include <thread>  
#include <boost/bind.hpp>

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

#include "../pj/eice/eice/src/json/json.h"
#include "test_common.h"
#include "../bridgeserver/AppServer.h"
#include "../bridgeserver/applog.h"
#include "../bridgeserver/MediaFilter.h"

#include "mock_conference_control.h"

using namespace std;

/*------------------------------------  DEFINITION ------------------------------------*/
/**
*	@brief	<b>Summary: start the application. </b>
*			<b>Detail: The main function should call this function to start the application.
this function will be call at Application::start() </b>
**************************************************************************/
/*----------------------------------- ENUMERATIONS ------------------------------------*/
/*-------------------------------------- CONSTANTS ------------------------------------*/
/*-------------------------- STRUCT/UNION/CLASS DATA TYPES ----------------------------*/
extern void increaseCount();
extern int  g_count;


class TestUtil : public CPPUNIT_NS::TestFixture {
	CPPUNIT_TEST_SUITE( TestUtil );
	CPPUNIT_TEST( test_thread_call );
	CPPUNIT_TEST(test_util );
	CPPUNIT_TEST_SUITE_END();

public:
	void setUp()
	{
		LOG_INFO("run to TestUtil.\n");		
	}
	void tearDown()
	{	
		LOG_INFO("quit to TestUtil.\n");
	}

public:
	REPEAREDTIMES_PROXY(TestUtil);

	void test_util()
	{
		CPPUNIT_ASSERT(g_app.iniFile().readInt("GLOBAL", "local_tcp_server_port", 20000) == 30002);

		char data[13] = "123456789012";
		std::string outs = toHexString(data, 12);
		CPPUNIT_ASSERT(outs.size() == 24);

		outs = toHexString(&data[4], 4);
		CPPUNIT_ASSERT(outs == toHexString(&data[4], 4));
		

		std::string iceJsonConfig_ = g_app.iniFile().top()("GLOBAL")["ice_json_config"];
		LOG_DEBUG("iceJsonConfig_  " << iceJsonConfig_);

		Json_em::Value configInfo_;
		Json_em::Reader config_reader;
		if (!config_reader.parse(iceJsonConfig_, configInfo_)) {
			LOG_WARN("parse ice json config fail!!!");
		}

		CPPUNIT_ASSERT(configInfo_.isMember("compCount"));

		configInfo_["comCount"] = Json_em::Value(4);
		CPPUNIT_ASSERT(configInfo_["comCount"].asInt() == 4);


		memset(data, 0, 20);
		rtp_hdr* curRtpHeader = (rtp_hdr*)&data;
		curRtpHeader->m = 1;
		LOG_DEBUG("data is " << toHexString(data, 4));
				
		CPPUNIT_ASSERT((data[1] & 0x80) == 0x80);
		//data[1] |= 0x80;

		memset(data, 0, 20);
		data[1] |= 0x80;
		LOG_DEBUG("data is: " << toHexString(data, 4));
	}
	

	static void increase_count(CCS* call) {
		for (int i = 0; i < 10; ++i)
		{
			call->call(increaseCount);
		}
	}

	void test_thread_call()
	{		
		try
		{
			LOG_INFO("Start.");

			CCS ccs;
			ccs.start();
			usleep(100000);
					

			g_count = 0;
			vector<std::thread*> threads;
			for (int i = 0; i < 10; ++i)
			{
				threads.push_back(new std::thread(increase_count, &ccs));
			}

			for (int i = 0; i < 10; ++i) {
				threads[i]->join();
				delete threads[i];
			}

			usleep(500000);

			LOG_INFO("Current gCount is " << g_count);
			CPPUNIT_ASSERT(g_count == 100);
			
			//ccs.call(std::bind(&CCS::stop, &ccs));
			ccs.stop();			
		}
		catch (...)
		{
			LOG_INFO("exception.");
		}
	}
};


/*------------------------------------- GLOBAL DATA -----------------------------------*/
/*----------------------------- LOCAL FUNCTION PROTOTYPES -----------------------------*/
/*-------------------------------- FUNCTION DEFINITION --------------------------------*/
void start_test_util(CppUnit::TextUi::TestRunner& runner) {
	runner.addTest(TestUtil::suite());
}
//CPPUNIT_TEST_SUITE_REGISTRATION( REPEAREDTIMES_REG(TestUtil, 1) );
