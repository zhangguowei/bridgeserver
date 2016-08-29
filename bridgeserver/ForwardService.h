#pragma once

#include <string>
#include <map>
#include <vector>

#include <time.h>
#include <stdint.h>

#include "event2/event_struct.h" 

#include "IceService.h"
#include "MediaFilter.h"

static const int FORWARD_ALIVE_CHECK_TIME = 10 * 60;	/**10 minute.*/
static const int MAX_UDP_PACKET_LEN = 10 * 1024;

enum MEDIA_TYPE {
	MEDIA_AUDIO,
	MEDIA_VIDEO
};

static const int AUDIO_RATIO_WEBRTC2XMPP = -3;
/**
一开始得到的地址都是客户端的本地地址，从收到的数据包中得到的是地址是客户端的出口地址。
目前，暂时认为webrtc client中得到的地址本地和出口是相同的； 以后通过地址router探测来定位。
*/
class ForwardService {
	friend class TestForward;
public:
	struct Address
	{
		std::string	ip;
		uint16_t	port;
		
		bool operator==(const Address &addr) const
		{
			return (addr.ip == ip) && (addr.port == port);
		};
		bool operator<(const Address& addr) const 
		{
			return (ip < addr.ip) || (port < addr.port);
		}
	};
	struct Forward {
		AddrPair		iceInfo;
		MEDIA_TYPE		mediaType;

		sockaddr_in		webrtcAddr;
		sockaddr_in		xmppAddr;
						
		event*			ev_udp;

		char			buffer[MAX_UDP_PACKET_LEN];

		time_t			lastPackageTime;	/**long time(for example: 10min) no package, will terminate it.*/

		VideoRtpFilter	videoFilter;
		AudioRtpFilter	audioFilter;

		bool isWebrtcAddr(const struct sockaddr_in& tempadd);

		Forward() : lastPackageTime(time(NULL)) {}
		~Forward();
	};
	typedef std::map<Address, Forward> ForwardMap;
			
	ForwardService();
	~ForwardService() {}

	void startForward(MEDIA_TYPE media, AddrPair& addr_pairs, const sockaddr_in& webrtcAddr);
		
private:
	static void udpCallback(evutil_socket_t fd, short what, void *arg);
	static void timeout_cb(evutil_socket_t fd, short event, void *arg);
		

	Forward* findForward(const Address& local) {
		ForwardMap::iterator it = forwards_.find(local);
		return (it == forwards_.end()) ? NULL : &it->second;
	}

private:	
	struct event	timer_;		
		
	ForwardMap		forwards_;
};

