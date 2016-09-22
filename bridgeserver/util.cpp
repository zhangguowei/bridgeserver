#include "event2/event.h"  
#include "event2/event_struct.h"  
#include "event2/util.h"  
#include "event2/buffer.h"  
#include "event2/listener.h"  
#include "event2/bufferevent.h"  

#include <string.h>

#include <assert.h>

#include "../bridgeserver/applog.h"
#include "util.h"

std::string toHexString(const char* buf, int len, const std::string& tok)
{
	std::string output;
	output.reserve(len * (2 + tok.size()));
	char temp[8];
	for (int i = 0; i < len; ++i)
	{
		sprintf(temp, "%.2x", (uint8_t)buf[i]);
		output.append(temp, 2);
		output.append(tok);
	}

	return output;
}

/** @desc create a timer		
*@param[out] timer		timer event object for this timer
*@param[in] eventBase	event base object which this timer is run in.
*@param[in] time (s)	time for this timer
*@param[in] cb			callback for this timer
*@param[in] arg			argument for cb
*@return true: success to create a timer.
*/
bool startTimer(struct event& timer, event_base* eventBase, int time, Timout_cb cb, void *arg)
{
	struct timeval tv;
	if (0 != event_assign(&timer, eventBase, -1, EV_PERSIST, cb, arg))
		return false;
	evutil_timerclear(&tv);
	tv.tv_sec = time;
	if (0 != event_add(&timer, &tv))
		return false;

	return true;
}

int createUdpSocket(int port)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	int sock;
	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
		LOG_WARN("create socket fail, port: " << port);
		return -1;
	}

	int reuseaddr_on = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_on, sizeof(reuseaddr_on)) == -1)
	{
		LOG_WARN("fail to set socket reuse, port: " << port);
	}

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0){
		LOG_WARN("bind socket fail, port: " << port);
		return -1;
	}
	return sock;
}
