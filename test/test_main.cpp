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
* @version 0.1.1
*
* Revision History 
* @if  CR/PR ID Author	   Date		  Major Change @endif
*               shangrong.rao May. 22 2015 create version 0.0.1\n
****************************************************************************************/
/*------------------------------- HEADER FILE INCLUDES ---------------------------------*/
#include <thread>  
#include <functional>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include "../bridgeserver/applog.h"

#include "../bridgeserver/AppServer.h"

AppServer g_app;

using namespace log4cplus;   
using namespace log4cplus::helpers;   
/*------------------------------------  DEFINITION ------------------------------------*/
/*----------------------------------- ENUMERATIONS ------------------------------------*/
/*-------------------------- STRUCT/UNION/CLASS DATA TYPES ----------------------------*/
/*-------------------------------------- CONSTANTS ------------------------------------*/
/*------------------------------------- GLOBAL DATA -----------------------------------*/
/*----------------------------- LOCAL FUNCTION PROTOTYPES -----------------------------*/
extern void start_test_ice(CppUnit::TextUi::TestRunner& runner);
extern void start_test_forward(CppUnit::TextUi::TestRunner& runner);
extern void start_test_util(CppUnit::TextUi::TestRunner& runner);
/*-------------------------------- FUNCTION DEFINITION --------------------------------*/
/**
* @brief 主函数，仅仅用于示例
* @details 系统启动后，经过初始化，第一个用户函数调用，
*          就调用该函数，当该函数返回时系统退出该程序。
***********************************************************************************/
int main( int argc, char ** argv) {			
	LOG_INFO("Start.");

	std::thread appThread(std::bind(&AppServer::start, &g_app));
	usleep(100000);			/// wait complete g_app start

	CppUnit::TextUi::TestRunner runner;
		
	// setting the order of these tests.		
	start_test_util(runner);
	start_test_forward(runner);
	start_test_ice(runner);
	
	LOG_INFO("------------------come main 1.");

	// add the test suit needn't order.
	runner.addTest(CPPUNIT_NS::TestFactoryRegistry::getRegistry().makeTest());

	bool ret = !runner.run();

	usleep(20000000);

	LOG_INFO("------------------come main 2.");

	g_app.call(std::bind(&AppServer::stop, &g_app));	
	appThread.join();
}

