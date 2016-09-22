

// command:
// interface:
// 1. prepareSession(SessionID, localInfo)
// 2. startSession(SessionID, peerInfo)
// 3. joinSession(SessionID, peerInfo)
// 4. relaySession(SessionID, webrtcAnswer)	


// data:  relay data between kurento and mobile

#include "applog.h"
#include "AppServer.h"

#include <unistd.h>
#include <stdio.h>

AppServer g_app;

int main(int argc, char *argv[])
{
	//使用damon，在后台运行
	int iNoClose = 0;
	//if (daemon(1, iNoClose) == -1) 
	//{
	//	perror("daemon() fail.");
	//	return -1;
	//}
			
	g_app.start();

    return 0;
}
