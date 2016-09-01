#pragma once  

#include <map>
#include <set>
#include <vector>  

#include "event2/event_struct.h"

#include <assert.h>

struct event_base;
struct bufferevent;
class TcpServer;

class Connection
{
public:	
	Connection(struct bufferevent *bev);
	virtual ~Connection();

	virtual void handleCommand(const char *pDataBuffer, int nLength) = 0;

	/**
	@desc send data to this connection.
	@return fail: -1; success: 0
	*/
	int send(const void *data, unsigned len);
		
	void setServer(TcpServer* server);

private:
	Connection(const Connection&);
	Connection& operator=(const Connection&);
	static void readCallback(struct bufferevent *bev, void *data);
	static void writeCallback(struct bufferevent *bev, void *data);
	static void eventCallback(struct bufferevent *bev, short what, void *data);
	
private:	
	TcpServer*			server_;

	struct bufferevent *bufferEvent_;

	bool				canSend_;
};

class ConnectionBuilder{
public:
	virtual ~ConnectionBuilder() {}
	virtual Connection* create(bufferevent *bev) = 0;
};
