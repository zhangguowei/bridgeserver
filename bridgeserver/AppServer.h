#pragma once  

#include <functional>

#include <memory>

#include "ini.hpp"

#include "event2/event_struct.h" // for: struct event  
#include "libevent_thread_call.h"
#include "TcpServer.h"

struct event_base;
struct bufferevent;

class TcpServer;
class IceService;
class ForwardService;
class IceConnection;
namespace INI {
	class Parser;
}
typedef INI::Parser IniFile;

class AppServer
{
public:
	AppServer();
	~AppServer();

	void start();
	void stop();

	/**
	* @desc other thread call function at this(main) thread.
	*/
	void call(const Functor& f);
	
	event_base* eventBase() const { return eventBase_; }
		
	IniFile& iniFile() { return *iniFile_.get(); }

	TcpServer* iceServer()			{ return iceServer_.get(); }
	IceService* iceService()		{ return iceService_.get(); }
	ForwardService* forwardService(){ return mediaForwardService_.get(); }

private:
	static void ev_signal_cb(evutil_socket_t sig, short events, void *user_data);
private:
	struct event_base *						eventBase_;				/** libevent for main thread to handle business.*/
	std::unique_ptr<TcpServer>				iceServer_;				
	std::unique_ptr<IceService>				iceService_;			/** provide ice nat penetrate service */
	std::unique_ptr<ForwardService>			mediaForwardService_;	/** forward media between webrtc user and xmpp user.*/

	std::unique_ptr<EventThreadCall>		eventThreadCall_;		/**for call by other thread*/
		
	std::unique_ptr<INI::Parser>			iniFile_;
};

extern AppServer g_app;
