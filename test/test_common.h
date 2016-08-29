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

#ifndef __TEST_COMMON_H__
#define __TEST_COMMON_H__
/*------------------------------- HEADER FILE INCLUDES --------------------------------*/
/*------------------------------------  DEFINITION ------------------------------------*/
#define REPEAREDTIMES_PROXY(CLASS) \
template<int count> \
class Repeated \
{ \
public:  \
	static CPPUNIT_NS::Test* suite() \
	{ \
	return new CPPUNIT_NS::RepeatedTest(CLASS::suite(), count); \
	} \
}
#define REPEAREDTIMES_REG(CLASS,Times) CLASS::Repeated<Times>
/*----------------------------------- ENUMERATIONS ------------------------------------*/
/*-------------------------- STRUCT/UNION/CLASS DATA TYPES ----------------------------*/
/*-------------------------------------- CONSTANTS ------------------------------------*/
/*------------------------------------- GLOBAL DATA -----------------------------------*/
/// what kind of service this test used for.
//extern bool g_is_audio_service;
/*----------------------------- LOCAL FUNCTION PROTOTYPES -----------------------------*/
/*-------------------------------- FUNCTION DEFINITION --------------------------------*/

#endif /*__TEST_COMMON_H__*/
