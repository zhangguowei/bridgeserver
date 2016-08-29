/****************************************************************************************
* Copyright (c) 2015~2015 All Rights Resverved by
*  . 
****************************************************************************************/
/**
* @file applog.h
* @brief 
*
* @author 
*
* @date 
*
* @version 0.1.1
*
* Revision History 
* @if  CR/PR ID Author     Date       Major Change @endif
*               shangrong.rao  create version 0.1\n
****************************************************************************************/
#ifndef __APPLOG_H__
#define __APPLOG_H__
/*------------------------------- HEADER FILE INCLUDES --------------------------------*/
//#include <boost/pool/detail/singleton.hpp>

#include <log4cplus/loggingmacros.h>
#include <log4cplus/logger.h>   
#include <log4cplus/fileappender.h>   
#include <log4cplus/consoleappender.h>   
#include <log4cplus/layout.h>   
#include <log4cplus/configurator.h>
#include "util.h"
/*------------------------------------  DEFINITION ------------------------------------*/
/*
#define STDOUT std::cout << __FILE__ << " " << __FUNCTION__ << "  " << __LINE__ << "---  "
#define LOG_FATAL(arg)  STDOUT << arg << std::endl;
#define LOG_ERROR(arg)  STDOUT << arg << std::endl;
#define LOG_WARN(arg)   STDOUT << arg << std::endl;
#define LOG_INFO(arg)   STDOUT << arg << std::endl;
#define LOG_DEBUG(arg)  STDOUT << arg << std::endl;
#define LOG_TRACE(arg)  STDOUT << arg << std::endl;
*/

#define LOG_FATAL(arg)  LOG4CPLUS_MACRO_BODY(g_appLogger, arg, FATAL)
#define LOG_ERROR(arg)  LOG4CPLUS_MACRO_BODY(g_appLogger, arg, ERROR)
#define LOG_WARN(arg)   LOG4CPLUS_MACRO_BODY(g_appLogger, arg, WARN)
#define LOG_INFO(arg)   LOG4CPLUS_MACRO_BODY(g_appLogger, arg, INFO)
#define LOG_DEBUG(arg)  LOG4CPLUS_MACRO_BODY(g_appLogger, arg, DEBUG)
#define LOG_TRACE(arg)  LOG4CPLUS_MACRO_BODY(g_appLogger, arg, TRACE)

/*----------------------------------- ENUMERATIONS ------------------------------------*/
/*-------------------------- STRUCT/UNION/CLASS DATA TYPES ----------------------------*/

using namespace log4cplus;
/*-------------------------------------- CONSTANTS ------------------------------------*/
/*------------------------------------- GLOBAL DATA -----------------------------------*/
class SWLog
{
public:
    SWLog() : appLogger_(Logger::getRoot())
    {
        PropertyConfigurator::doConfigure(LOG4CPLUS_TEXT("log4cplus.properties"));          
    }

    log4cplus::Logger appLogger_;
};

//Logger g_appLogger = Logger::getInstance("LoggerName");
#define  g_appLogger (SingleInstance<SWLog>::instance().appLogger_)

/*----------------------------- LOCAL FUNCTION PROTOTYPES -----------------------------*/
/*-------------------------------- FUNCTION DEFINITION --------------------------------*/

#endif /*__APPLOG_H__*/
