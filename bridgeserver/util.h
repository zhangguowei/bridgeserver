#pragma once

#include <string>

#include "event2/event_struct.h" 

template <class T> 
class SingleInstance
{
    static T *instance_;
public:
    static T & instance()
    {
        if (instance_ == NULL)
        {
            instance_ = new T;
        }
        return *instance_;
    }
};
template <class T> T *SingleInstance<T>::instance_ = NULL; 


std::string toHexString(const char* buf, int len, const std::string& tok = "");


typedef void(*Timout_cb)(evutil_socket_t fd, short event, void *arg);
/**
* @desc: create a timer		
* @param[out]	timer: timer event
* @param[in]	eventBase: libevent event base
* @param[in]	time: time of timer, unit: s
* @param[in]	cb: callback function.
* @param[in]	arg: callback parameter
*/
bool startTimer(struct event& timer, event_base* eventBase, int time, Timout_cb cb, void *arg);

/**
* @desc: create a udp socket
* @param[in]	port: port used by this socket
* @return		-1: fail; socket : success;
*/
int createUdpSocket(int port);


