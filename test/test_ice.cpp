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

#include "mock_conference_control.h"

using namespace std;

extern "C" {
	int eice_test();
}
extern event_base* g_ccs_eventBase;
extern void startCCS();
extern void stopCCS();

/*------------------------------------  DEFINITION ------------------------------------*/
/*----------------------------------- ENUMERATIONS ------------------------------------*/
/*-------------------------------------- CONSTANTS ------------------------------------*/
/*-------------------------- STRUCT/UNION/CLASS DATA TYPES ----------------------------*/

class TestIce : public CPPUNIT_NS::TestFixture {
	CPPUNIT_TEST_SUITE( TestIce );
	CPPUNIT_TEST( test_ice );
	CPPUNIT_TEST(test_css);
	CPPUNIT_TEST(test_concurrency);
	CPPUNIT_TEST_SUITE_END();

public:
	void setUp()
	{
		printf("run to TestIce.\n");		
	}
	void tearDown()
	{	
		printf("quit to TestIce.\n");
	}
	
public:
	REPEAREDTIMES_PROXY(TestIce);

	void test_ice() {
		// eice_test();  不能重复初始化

		string iceJsonConfig = g_app.iniFile().top()("GLOBAL")["ice_json_config"];
		CPPUNIT_ASSERT(!iceJsonConfig.empty());
	}

	// 整体的集成测试	
	void test_css()
	{
		for (int i = 0; i < 5; ++i)
		{
			CCS ccs;
			ccs.start();
			usleep(500000);

			// ccs start ice
			ccs.call(std::bind(&CCS::startACaller, &ccs));

			usleep(6000000);
			ccs.stop();
		}
	}

	void test_concurrency()
	{
		//return;
		const int COUNT = 20;
		CCS ccs[COUNT];
		for (int i = 0; i < COUNT; ++i)
		{
			ccs[i].start();
		}
		usleep(800000);

		for (int i = 0; i < COUNT; ++i)
		{
			LOG_INFO("Start thread " << i << " to test.");
			ccs[i].call(std::bind(&CCS::startACaller, &ccs[i]));

			struct timeval tpstart;
			gettimeofday(&tpstart, NULL);
			srand(tpstart.tv_usec);
			int n = (1 + ((unsigned)(50.0*rand()) / (RAND_MAX + 1.0)));
			usleep(n);			
		}
		
		usleep(12000000);
		LOG_INFO("Now will clear csses.");
		for (int i = 0; i < COUNT; ++i)
		{
			ccs[i].stop();
		}
	}
};


/*------------------------------------- GLOBAL DATA -----------------------------------*/
/*----------------------------- LOCAL FUNCTION PROTOTYPES -----------------------------*/
/*-------------------------------- FUNCTION DEFINITION --------------------------------*/
void start_test_ice(CppUnit::TextUi::TestRunner& runner) {
	runner.addTest(TestIce::suite());
}
//CPPUNIT_TEST_SUITE_REGISTRATION( REPEAREDTIMES_REG(TestIce, 1) );
