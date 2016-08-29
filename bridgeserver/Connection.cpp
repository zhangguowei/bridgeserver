#include "event2/event.h"  
#include "event2/util.h"  
#include "event2/buffer.h"  
#include "event2/listener.h"  
#include "event2/bufferevent.h"  

#include <iostream>  
#include <string.h>

#include "AppServer.h"
#include "applog.h"
#include "TcpServer.h"
#include "Connection.h"

Connection::Connection(bufferevent* bev)
	: bufferEvent_(bev)
	, canSend_(false)
{
	bufferevent_setcb(bufferEvent_, readCallback, writeCallback, eventCallback, this);
	if (0 != bufferevent_enable(bufferEvent_, EV_READ | EV_WRITE)) {
		LOG_ERROR("bufferevent enable error");
	}
}

Connection::~Connection()
{
	if (bufferEvent_)	bufferevent_free(bufferEvent_);
}

void Connection::readCallback(struct bufferevent *bev, void *data)
{
	assert(data != NULL);
	Connection* self = (Connection*)data;
	assert(bev == self->bufferEvent_);

	char msg[4096];
	size_t len;
	while ((len = bufferevent_read(bev, msg, sizeof(msg) - 1)) > 0)
	{
		self->handleCommand(msg, len);
	}
}

int Connection::send(const void *data, unsigned len)
{	
	return bufferevent_write(bufferEvent_, data, len);
}
void Connection::writeCallback(struct bufferevent *bev, void *data)
{	
	assert(data != NULL);
	Connection* self = (Connection*)data;
	assert(bev == self->bufferEvent_);
	self->canSend_ = true;
}

void Connection::setServer(TcpServer* server)
{
	server_ = server;
}

void Connection::eventCallback(struct bufferevent *bev, short what, void *data)
{
	assert(data != NULL);
	Connection *self = (Connection*)data;

	evutil_socket_t fd = bufferevent_getfd(bev);	
	LOG_INFO("EventCallback, fd: " << fd << "; type: " << what);

	if (what & BEV_EVENT_READING) {
		LOG_INFO("BEV_EVENT_READING...");
		return;
	}
	if (what & BEV_EVENT_WRITING) {
		LOG_INFO("BEV_EVENT_WRITING...");
		return;
	}	
	if (what & BEV_EVENT_CONNECTED) {
		LOG_INFO("BEV_EVENT_CONNECTED...");
		return;
	}

	if (what & BEV_EVENT_ERROR) {
		LOG_INFO("BEV_EVENT_ERROR, some other error: " << EVUTIL_SOCKET_ERROR());
	}
	if (what & BEV_EVENT_EOF) {
		LOG_INFO("BEV_EVENT_EOF, connection closed: " << EVUTIL_SOCKET_ERROR());
	}
	if (what & BEV_EVENT_TIMEOUT) {
		LOG_INFO("BEV_EVENT_TIMEOUT...");	
	}
		
	/// tell server to delete this conn
	if (self->server_ != NULL) self->server_->closeConnection(self);
}
