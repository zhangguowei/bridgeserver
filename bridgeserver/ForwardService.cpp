#include "event2/event.h"  
#include "event2/event_struct.h"  
#include "event2/util.h"  
#include "event2/buffer.h"  
#include "event2/listener.h"  
#include "event2/bufferevent.h"  

#include <string.h>

#include <assert.h>
#include <arpa/inet.h>

#include "applog.h"
#include "AppServer.h"
#include "ForwardService.h"



void ForwardService::startForward(MEDIA_TYPE media, AddrPair& iceInfo, const sockaddr_in& webrtcAddr)
{
	assert(iceInfo.local_port > 0);
		
	LOG_INFO("Start forward, local " << iceInfo.local_ip << ":" << iceInfo.local_port
		<< "; remote " << iceInfo.remote_ip << ":" << iceInfo.remote_port
		<< "; webrtc addr: " << toHexString((char*)&webrtcAddr, sizeof(sockaddr_in)));

	if (iceInfo.local_port == 0){
		LOG_WARN("error local port " << iceInfo.local_port);
		return;
	}

	int local_fd = (iceInfo.local_fd > 0) ? iceInfo.local_fd : createUdpSocket(iceInfo.local_port);
	if (local_fd <= 0){
		LOG_WARN("error local fd from ice result.");
		return;
	}

	Address local;
	local.ip = iceInfo.local_ip;
	local.port = iceInfo.local_port;	
	if (forwards_.find(local) != forwards_.end()){
		LOG_WARN("This local address has already used!, ip:" << local.ip << ", port:" << local.port);
		return;
	}	
	evutil_make_socket_nonblocking(local_fd);
	
	Forward& forward = forwards_[local];
	
	forward.ev_udp = event_new(g_app.eventBase(), local_fd, EV_READ | EV_PERSIST, udpCallback, &forward);
	if (!forward.ev_udp) {
		LOG_ERROR("Cann't new event for udp transport.");
		forwards_.erase(local);
		return;
	}

	event_add(forward.ev_udp, NULL);

	forward.iceInfo = iceInfo;	
	forward.mediaType = media;
	forward.webrtcAddr = webrtcAddr;	
	memset(&forward.xmppAddr, 0, sizeof(struct sockaddr_in));
	forward.xmppAddr.sin_family = AF_INET;
	forward.xmppAddr.sin_port = htons(forward.iceInfo.remote_port);
	forward.xmppAddr.sin_addr.s_addr = inet_addr(forward.iceInfo.remote_ip.c_str());

	forward.xmppVideoResend.setUdpSendFunctor(std::bind(::sendto, local_fd, std::placeholders::_1, std::placeholders::_2, 0, (struct sockaddr*)&forward.xmppAddr, sizeof(struct sockaddr_in)));	
//	forward.xmppVideoResend.setUdpSendFunctor([=](const char* data, int len){
//		return sendto(local_fd, data, len, 0, (struct sockaddr*)&forward.xmppAddr, sizeof(struct sockaddr_in));
//	});
}

/// 清理不使用的会话
void ForwardService::timeout_cb(evutil_socket_t fd, short event, void *arg)
{
	assert(arg != NULL);
	time_t curTime = time(NULL);
	ForwardService* fs = (ForwardService*)arg;
	for (ForwardMap::iterator it = fs->forwards_.begin(); it != fs->forwards_.end();)
	{
		Forward& forward = it->second;
		if ((curTime - forward.lastPackageTime) > FORWARD_ALIVE_CHECK_TIME) {
			LOG_INFO("This media session type:" << forward.mediaType				
				<< "; webrtc addr: " << toHexString((char*)&forward.webrtcAddr, sizeof(sockaddr_in))
				<< "; xmpp addr: " << toHexString((char*)&forward.xmppAddr, sizeof(sockaddr_in))
				<< " is not alive for: " << curTime - forward.lastPackageTime << "s, will remove it!");
			fs->forwards_.erase(it++);
		}
		else
			++it;
	}
}


ForwardService::ForwardService() {
	/// create a timer		
	struct timeval tv;
	event_assign(&timer_, g_app.eventBase(), -1, EV_PERSIST, timeout_cb, this);
	evutil_timerclear(&tv);
	tv.tv_sec = FORWARD_ALIVE_CHECK_TIME;
	event_add(&timer_, &tv);
}

bool ForwardService::Forward::isWebrtcAddr(const struct sockaddr_in& tempadd)
{
	if (memcmp(&webrtcAddr, &tempadd, sizeof(tempadd)) == 0)
		return true;
	else if (webrtcAddr.sin_port == xmppAddr.sin_port) {
		// do more...
		return false;
	}
	else {
		return tempadd.sin_port == webrtcAddr.sin_port;
	}
}

void ForwardService::udpCallback(evutil_socket_t fd, short what, void *arg)
{	
	if (!(what&EV_READ)) return;

	assert(arg != NULL);
	Forward * forward = (Forward*)arg;

	struct sockaddr_in tempadd;
	ev_socklen_t addrLen = sizeof(tempadd);
	
	while (true)
	{
		int dataLen = recvfrom(fd, forward->buffer, MAX_UDP_PACKET_LEN, 0, (struct sockaddr*)&tempadd, &addrLen);
		if (dataLen <= 0){
			break;
		}
		forward->lastPackageTime = time(NULL);
		
		//LOG_DEBUG("receive data len: " << dataLen << "; from " << ntohs(tempadd.sin_port));				
		//LOG_DEBUG("from addr: " << toHexString((const char*)&tempadd, addrLen)
		//	<< "; webrtc addr: " << toHexString((char*)&forward->webrtcAddr, addrLen)
		//	<< "; xmpp addr: " << toHexString((char*)&forward->xmppAddr, addrLen));

		// forward received data
		if (forward->isWebrtcAddr(tempadd)) {
			if (forward->mediaType == MEDIA_VIDEO) {				
				if (!forward->videoFilter.filter(forward->buffer, dataLen))
					assert(false);// continue;
				forward->xmppVideoResend.cacheData(forward->buffer, dataLen);
			} 
			else if (forward->mediaType == MEDIA_AUDIO) {
				if (!forward->audioFilter.filter(forward->buffer, dataLen, AUDIO_RATIO_WEBRTC2XMPP))
					assert(false);
				//	continue;
			}
			
			sendto(fd, forward->buffer, dataLen, 0, (struct sockaddr*)&forward->xmppAddr, addrLen);
			LOG_DEBUG("send data len: " << dataLen << "; from addr: " << toHexString((const char*)&tempadd, addrLen) << " to xmpp, port: " << ntohs(forward->xmppAddr.sin_port));
		}
		else {
			if (forward->mediaType == MEDIA_VIDEO) {
				// 如果是xmpp的重发请求，直接处理掉
				if (forward->xmppVideoResend.handle(forward->buffer, dataLen))
					continue;
			} else if (forward->mediaType == MEDIA_AUDIO) {
				if (!forward->audioFilter.filter(forward->buffer, dataLen, -AUDIO_RATIO_WEBRTC2XMPP))
					continue;
			}

			sendto(fd, forward->buffer, dataLen, 0, (struct sockaddr*)&forward->webrtcAddr, addrLen);
			LOG_DEBUG("send data len: " << dataLen << "; from addr: " << toHexString((const char*)&tempadd, addrLen) << " to webtrc, port: " << ntohs(forward->webrtcAddr.sin_port));
		}
	}
}


ForwardService::Forward::~Forward() {
	if (iceInfo.local_fd > 0){
		evutil_closesocket(iceInfo.local_fd);
	}
	if (ev_udp != NULL) {
		event_free(ev_udp);
	}
}

