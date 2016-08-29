#pragma once

#include <thread>
#include <memory>

#include "../bridgeserver/libevent_thread_call.h"

class CcsConnection;
class TcpServer;

class CCS {
public:
	friend CcsConnection;

	CCS();
	~CCS();
	void startConferenceControlServer();
	void start();
	void stop();
	void stop_in();

	void startACaller();

	struct event_base * eventBase() const { return eventBase_; }
	
	void call(const Functor& f);

	static void timeout_cb(evutil_socket_t fd, short event, void *arg);

private:
	struct event_base *	eventBase_;
	CcsConnection*		bridgeClient_;

	EventThreadCall*	threadCall_;

	std::thread*		thread_;

	std::unique_ptr<TcpServer>	webrtcServer_;	// for send offer and get answer

	struct event		timeout_;
	unsigned			start_flag_;
};