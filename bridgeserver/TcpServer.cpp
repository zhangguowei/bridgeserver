#include "event2/event.h"  
#include "event2/util.h"  
#include "event2/buffer.h"  
#include "event2/listener.h"  
#include "event2/bufferevent.h"  

#include <iostream>  
#include <string.h>

#include "AppServer.h"
#include "applog.h"
#include "Connection.h"
#include "TcpServer.h"


TcpServer::TcpServer(unsigned short port, event_base *base, ConnectionBuilder* builder)
	: port_(port)
	, connectionBuilder_(builder)
{
	assert(builder != NULL);

	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(0);
	sin.sin_port = htons(port_);

	listener_ = evconnlistener_new_bind(base, acceptCallback, this, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1, (sockaddr*)&sin, sizeof(sin));
	if (listener_ == NULL) {
		LOG_ERROR("Fail to start tcp server!");
	}
}
TcpServer::~TcpServer()
{
	if (listener_)	evconnlistener_free(listener_);
	delete connectionBuilder_;
}

bool TcpServer::haveConnection(Connection* conn) const
{
	return connections_.find(conn) != connections_.end();
}
void TcpServer::closeConnection(Connection* conn)
{
	std::set<Connection*>::iterator it = connections_.find(conn);
	assert(it != connections_.end());
	//{		LOG_WARN("No this connection!");		return;	}
	delete *it;
	connections_.erase(it);
}

void TcpServer::acceptCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *addr, int socklen, void *arg)
{
	assert(arg != NULL);

	LOG_INFO("new connection is comming.");

	TcpServer *self = (TcpServer*)arg;

	bufferevent *bev = bufferevent_socket_new(g_app.eventBase(), fd, BEV_OPT_CLOSE_ON_FREE);
	Connection *conn = self->connectionBuilder_->create(bev);
	if (conn == NULL) {
		LOG_WARN("No enough memory to new a connection.");
		return;
	}
	self->connections_.insert(conn);
}
