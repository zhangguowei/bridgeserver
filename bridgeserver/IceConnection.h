#pragma once

#include <string>
#include <stdint.h>

#include "Connection.h"

// CCS（conference control server） 会议控制服务器
// BS（bridge server)				桥接服务器（包括ice和媒体转发）
// 1. webrtc as caller： PREPARE_SESSION	LOCAL_INFO  START_SESSION	RELAY_SESSION	ANSWER_SESSION
// 2. webrtc as callee： JOIN_SESSION		LOCAL_INFO	RELAY_SESSION	ANSWER_SESSION
enum ICE_OP {
	ICE_OP_NON				= 0,	// 初始化状态
	ICE_OP_PREPARE_SESSION	= 1,	// CCS 通知 BS 发起一个会话
	ICE_OP_LOCAL_INFO		= 2,	// BS 发起会话后，将本地ice地址信息通过CCS送到对方端点
	ICE_OP_START_SESSION	= 3,	// 对方端点将自己的ice地址信息通过CCS发送到BS
	ICE_OP_JOIN_SESSION		= 4,	// 如果自己是被叫方，则是CCS通知自己直接启动ice协商
	ICE_OP_ICE_RESULT		= 5,	// 将ICE协商结果告知CCS，方便CCS启动webrtc媒体转发
	ICE_OP_RELAY_SESSION	= 6,	// CCS将wetrtc的answer结果告知BS
	ICE_OP_STOP_SESSION		= 7,	// CCS通知BS结束一个转发会话
	ICE_OP_END				= 50	// 结束
};

std::string enumKey(ICE_OP value);

typedef uint64_t SessionID;

// 2字节信令编码
// 2字节内容长度
// 后面就是内容了(json格式)
struct IceCommand
{
	uint32_t		op;
	SessionID		sessionId;
	std::string		content;
};
static const uint16_t ICE_COMMAND_HEADER_LEN = 16;	/**op 4 bytes + sessionId 8 bytes + content length 4 bytes.*/

class IceConnectionBuilder :public ConnectionBuilder {
	virtual Connection* create(bufferevent *bev);
};

class IceConnection : public Connection
{		
public:
	IceConnection(struct bufferevent *bev) : Connection(bev) {}
	virtual void handleCommand(const char *pDataBuffer, int nLength);
	
protected:
	virtual void handleIceCommand(const IceCommand &command);

private:	
	bool parseCommand(IceCommand &res, const char *data, unsigned length, unsigned & usedLen);
				
private:
	std::string		recvBuffer_;
};

