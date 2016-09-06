
#include <assert.h>

#include "applog.h"
#include "AppServer.h"
#include "IceService.h"
#include "util.h"
#include "IceConnection.h"

#define enumValue(key) case key: 	return #key;
std::string enumKey(ICE_OP value)
{
	switch (value)
	{
		enumValue(ICE_OP_NON);
		enumValue(ICE_OP_PREPARE_SESSION);
		enumValue(ICE_OP_LOCAL_INFO);
		enumValue(ICE_OP_START_SESSION);
		enumValue(ICE_OP_JOIN_SESSION);
		enumValue(ICE_OP_ICE_RESULT);
		enumValue(ICE_OP_RELAY_SESSION);
		enumValue(ICE_OP_STOP_SESSION);
		enumValue(ICE_OP_END);
	default:
		return "";
	}
}

Connection* IceConnectionBuilder::create(bufferevent *bev)
{
	return new(std::nothrow) IceConnection(bev);
}

void IceConnection::handleIceCommand(const IceCommand &command)
{
	g_app.iceService()->handleIceCommand(command, this);
}

void IceConnection::handleCommand(const char *pDataBuffer, unsigned int dataLen)
{
	assert(pDataBuffer != NULL && dataLen > 0);

	LOG_DEBUG("Received data " << dataLen << "; current buffer " << recvBuffer_.size());	
	LOG_DEBUG("Received data: " << toHexString(pDataBuffer, std::min((unsigned)16, dataLen + ICE_COMMAND_HEADER_LEN), " "));

	unsigned totalUsedLen = 0;
	unsigned usedLen = 0;
	recvBuffer_.append(pDataBuffer, dataLen);
	IceCommand command;
	while ((dataLen > totalUsedLen))
	{
		if (!parseCommand(command, recvBuffer_.data() + totalUsedLen, recvBuffer_.size() - totalUsedLen, usedLen))
		{
			break;
		}		
		totalUsedLen += usedLen;

		handleIceCommand(command);
	}
	assert(totalUsedLen <= recvBuffer_.size());

	// remove used data
	LOG_DEBUG("buffer len: " << recvBuffer_.size() << "; used " << totalUsedLen);
	if (totalUsedLen == recvBuffer_.size())
	{
		recvBuffer_.clear();
	}
	else
	{
		recvBuffer_.erase(0, totalUsedLen);
	}
	LOG_DEBUG("current buffer len " << recvBuffer_.size());
}

bool IceConnection::parseCommand(IceCommand &command, const char *pDataBuffer, unsigned nLength, unsigned & usedLen)
{
	assert(pDataBuffer != NULL && nLength > 0);
	
	if (nLength < ICE_COMMAND_HEADER_LEN) {
		LOG_DEBUG("receive data with length is little to " << ICE_COMMAND_HEADER_LEN);
		return false;
	}
	
	uint32_t contentLen = *(unsigned short*)&pDataBuffer[ICE_COMMAND_HEADER_LEN-4];
	if (contentLen > nLength - ICE_COMMAND_HEADER_LEN) {
		LOG_DEBUG("current received data(data len: " << nLength - ICE_COMMAND_HEADER_LEN << ", need: " << contentLen << ") is not enough to make a command.");
		return false;
	}

	usedLen = ICE_COMMAND_HEADER_LEN + contentLen;
	command.op = *(uint32_t*)pDataBuffer;
	command.sessionId = *(SessionID*)(pDataBuffer+4);
	command.content.assign(pDataBuffer+ICE_COMMAND_HEADER_LEN, contentLen);

	return true;	
}

