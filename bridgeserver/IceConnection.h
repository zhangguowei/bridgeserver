#pragma once

#include <string>
#include <stdint.h>

#include "Connection.h"

// CCS��conference control server�� ������Ʒ�����
// BS��bridge server)				�Žӷ�����������ice��ý��ת����
// 1. webrtc as caller�� PREPARE_SESSION	LOCAL_INFO  START_SESSION	RELAY_SESSION	ANSWER_SESSION
// 2. webrtc as callee�� JOIN_SESSION		LOCAL_INFO	RELAY_SESSION	ANSWER_SESSION
enum ICE_OP {
	ICE_OP_NON				= 0,	// ��ʼ��״̬
	ICE_OP_PREPARE_SESSION	= 1,	// CCS ֪ͨ BS ����һ���Ự
	ICE_OP_LOCAL_INFO		= 2,	// BS ����Ự�󣬽�����ice��ַ��Ϣͨ��CCS�͵��Է��˵�
	ICE_OP_START_SESSION	= 3,	// �Է��˵㽫�Լ���ice��ַ��Ϣͨ��CCS���͵�BS
	ICE_OP_JOIN_SESSION		= 4,	// ����Լ��Ǳ��з�������CCS֪ͨ�Լ�ֱ������iceЭ��
	ICE_OP_ICE_RESULT		= 5,	// ��ICEЭ�̽����֪CCS������CCS����webrtcý��ת��
	ICE_OP_RELAY_SESSION	= 6,	// CCS��wetrtc��answer�����֪BS
	ICE_OP_STOP_SESSION		= 7,	// CCS֪ͨBS����һ��ת���Ự
	ICE_OP_END				= 50	// ����
};

std::string enumKey(ICE_OP value);

typedef uint64_t SessionID;

// 4�ֽ��������
// 8�ֽڻỰID
// 4�ֽ����ݳ���
// �������������(json��ʽ)
struct IceCommand
{
	uint32_t		op;
	SessionID		sessionId;
	std::string		content;
};
static const unsigned ICE_COMMAND_HEADER_LEN = 16;	/**op 4 bytes + sessionId 8 bytes + content length 4 bytes.*/

class IceConnectionBuilder :public ConnectionBuilder {
	virtual Connection* create(bufferevent *bev);
};

class IceConnection : public Connection
{		
public:
	IceConnection(struct bufferevent *bev) : Connection(bev) {}
	virtual void handleCommand(const char *pDataBuffer, unsigned int nLength);
	
protected:
	virtual void handleIceCommand(const IceCommand &command);

private:	
	/**
	*@desc parse data, generate command
	*@param[out]	res:	generated command
	*@param[in]		data:	input command data
	*@param[in]		length:	input command data length
	*@param[in]		usedLen:used data len for generate command
	*@return		true:	success to generate a command.
	*/
	bool parseCommand(IceCommand &res, const char *data, unsigned length, unsigned & usedLen);
				
private:
	std::string		recvBuffer_;
};

