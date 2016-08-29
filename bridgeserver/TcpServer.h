#pragma once  

#include <map>
#include <set>
#include <vector>  

#include "event2/event_struct.h"

#include <assert.h>

struct event_base;
struct bufferevent;

class Connection;
class ConnectionBuilder;

class TcpServer
{	
public:
	TcpServer(unsigned short port, event_base *base, ConnectionBuilder* builder);
	~TcpServer();

	/**@desc whether this conn is not disconnected.*/
	bool haveConnection(Connection* conn) const;

	void closeConnection(Connection* conn);
	
private:
	TcpServer(const TcpServer&);
	TcpServer& operator=(const TcpServer&);
		
	static void acceptCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *addr, int socklen, void *arg);

	const std::set<Connection*>& connections() const { return connections_; }

private:
	unsigned short			port_;		/**local server port*/
	struct evconnlistener *	listener_;

	ConnectionBuilder*		connectionBuilder_;
	
	std::set<Connection*>	connections_;
};
