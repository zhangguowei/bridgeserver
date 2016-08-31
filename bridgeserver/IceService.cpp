#include <string.h>
#include <assert.h>

#include <arpa/inet.h>

#include "applog.h"

#include "third/eice/src/eice.h"

#include "IceConnection.h"
#include "TcpServer.h"
#include "AppServer.h"
#include "ForwardService.h"
#include "IceService.h"


bool checkStatus(ICE_OP cur_status, ICE_OP new_status)
{
	bool ret;
	switch (new_status)
	{
	case ICE_OP_NON:
		ret = false;
		break;
	case ICE_OP_PREPARE_SESSION:
		ret = cur_status == ICE_OP_NON;
		break;
	case ICE_OP_LOCAL_INFO:
		ret = cur_status == ICE_OP_JOIN_SESSION || cur_status == ICE_OP_PREPARE_SESSION;
		break;
	case ICE_OP_START_SESSION:
		ret = (cur_status == ICE_OP_PREPARE_SESSION) || (cur_status == ICE_OP_LOCAL_INFO);
		break;
	case ICE_OP_JOIN_SESSION:
		ret = cur_status == ICE_OP_NON;
		break;
	case ICE_OP_ICE_RESULT:
		break;
	case ICE_OP_RELAY_SESSION:
		return cur_status == ICE_OP_ICE_RESULT;
	case ICE_OP_STOP_SESSION:
		break;
	case ICE_OP_END:
		break;
	default:
		ret = true;
		break;
	}

	if (!ret) {
		LOG_WARN(enumKey(new_status) << " can not be commit after " << enumKey(cur_status));
	}

	return ret;
}

void get_addr_pairs_from_result(eice_t obj, const char * caller_result, std::vector<AddrPair>& addr_pairs){
	update_steal_sockets(obj);

	try {
		Json_em::Reader caller_reader;
		Json_em::Value caller_value;
		if (!caller_reader.parse(caller_result, caller_value)) {
			LOG_DEBUG("parse result fail!!!" << caller_value);			
			return;
		}

		Json_em::Value pairs_value = caller_value["pairs"];
		Json_em::Value relay_pairs_value = caller_value["relay_pairs"];
		if (!pairs_value.isNull()){
			for (int i = 0; i < pairs_value.size(); i++) {

				int port = pairs_value[i]["local"]["port"].asInt();
				int fd = eice_get_global_socket(obj, port);
				if (fd < 0) {
					LOG_DEBUG("fail to get socket at " << i << " , port " << port << "!!!");					
				//	break;
				}

				addr_pairs.push_back(AddrPair(pairs_value[i]["local"]["ip"].asString()
					, pairs_value[i]["local"]["port"].asInt()
					, pairs_value[i]["remote"]["ip"].asString()
					, pairs_value[i]["remote"]["port"].asInt()
					, fd));				

				AddrPair &ap = addr_pairs.back();
				LOG_DEBUG("store No." << addr_pairs.size()-1 << " pair fd=" << ap.local_fd << ", local_port=" << ap.local_port << ", remote_port=" << ap.remote_port);								
			}			
		}
	} 
	catch (...)
	{
		LOG_ERROR("Error json: " << caller_result);
	}
}

bool constructCommandBuffer(char* buffer, ICE_OP op, SessionID id, const char* data, uint32_t dataLen)
{
	if (dataLen + ICE_COMMAND_HEADER_LEN > MAX_COMMAND_LEN) {
		LOG_WARN("The data is too long(" << dataLen << ") for ice command, discard it.");
		return false;
	}

	*(uint32_t*)buffer = op;
	*(SessionID*)&buffer[4] = id;
	*(uint32_t*)&buffer[ICE_COMMAND_HEADER_LEN-4] = dataLen;
	if (data != NULL) {
		memcpy(buffer+ICE_COMMAND_HEADER_LEN, data, dataLen);
	}

	return true;
}


IceService::IceService()
{
//	iceJsonConfig_ = g_app.iniFile().ReadSessionStr("GLOBAL", "ice_json_config", "");	
	iceJsonConfig_ = g_app.iniFile().top()("GLOBAL")["ice_json_config"];	
	LOG_DEBUG("iceJsonConfig_  " << iceJsonConfig_);

	Json_em::Reader config_reader;	
	if (!config_reader.parse(iceJsonConfig_, configInfo_)) {
		LOG_WARN("parse ice json config fail!!!");
	}

	if (!startTimer(timer_, g_app.eventBase(), SESSION_ALIVE_CHECK_TIME, timeout_cb, this)) {
		LOG_ERROR("Cann't create timer for check session");
	}
}

const char* IceService::iceJsonConfig()
{
	Json_em::FastWriter writer;
	iceJsonConfig_ = writer.write(configInfo_);
	return iceJsonConfig_.c_str();
}
void IceService::sendCommand(IceSession& is, ICE_OP op, const char* data, uint32_t dataLen)
{
	if (dataLen > 0)
		LOG_DEBUG("Send command *** " << enumKey(op) << "; Session id: " << is.id << "; data len " << dataLen << "; data: " << (data==NULL?sendBuffer_+ICE_COMMAND_HEADER_LEN:data));
	else
		LOG_DEBUG("Send command *** " << enumKey(op) << "; Session id: " << is.id << "; data len " << dataLen);

	// tell sender local info
	if (!constructCommandBuffer(sendBuffer_, op, is.id, data, dataLen)) {
		is.setStatus(ICE_OP_END);
		return;
	}
	is.setStatus(op);
		
	/// if failed to send....
	if (is.request->send(sendBuffer_, dataLen + ICE_COMMAND_HEADER_LEN) != 0) {
		LOG_WARN("Can't send the command: " << op << "; all len: " << dataLen + ICE_COMMAND_HEADER_LEN);
	}
}

void IceService::readJsonInfo(const std::string& json)
{
	if (json.empty())
		return;

	try
	{		
		Json_em::Reader json_reader;
		Json_em::Value addrInfo;
		if (!json_reader.parse(json, addrInfo)) {
			LOG_WARN("parse json info: " << json << ", fail!!!");
			return;
		}

		if (addrInfo.isMember("compCount")) {
			configInfo_["comCount"] = addrInfo["comCount"];
		}
	}
	catch (...){
		LOG_WARN("parse json info: " << json << ", exception!!!");
	}
}

void IceService::handleIceCommand(const IceCommand &command, Connection* sender)
{
	LOG_INFO("Receive command *** " << enumKey((ICE_OP)command.op) << ", id: " << command.sessionId
		<< ", content: " << command.content << "; len: " << command.content.size());

	switch (command.op)
	{	
	case ICE_OP_PREPARE_SESSION:
	{
		readJsonInfo(command.content);
		LOG_DEBUG("config json info: " << iceJsonConfig());

		//SessionID sid = genSessionId();
		if (sessions_.find(command.sessionId) != sessions_.end())
		{
			LOG_INFO("This session with id: " << command.sessionId << " is already existed!");
			break;
		}
		IceSession& is = sessions_[command.sessionId];
		is.request = sender;
		is.id = command.sessionId;
		int content_len = 0;
		if (eice_new_caller(iceJsonConfig(), sendBuffer_ + ICE_COMMAND_HEADER_LEN, &content_len, &is.endpoint) != 0)
		{
			LOG_WARN("something wrong with new ice endpoint !!!");
			is.setStatus(ICE_OP_END);
			break;
		}

		// tell sender local info
		sendCommand(is, ICE_OP_LOCAL_INFO, NULL, content_len);
		assert(sessions_.find(is.id) != sessions_.end());
	}
		break;
	case ICE_OP_START_SESSION:
	{		
		IceSessionMap::iterator it = sessions_.find(command.sessionId);
		if (it == sessions_.end()) {
			LOG_WARN("Received a START_SESSION with inexistent sessionId:" << command.sessionId << "; session count: " << sessions_.size());			
			break;
		}
		IceSession& is = it->second;

		if (!checkStatus((ICE_OP)is.curStatus, ICE_OP_START_SESSION))
			break;		

		int ret = eice_caller_nego(is.endpoint, command.content.c_str(), command.content.size(), 0, 0);
		LOG_INFO("eice_caller_nego return " << ret);
		if (ret != 0) {
			LOG_WARN("something wrong with new caller nego !!!");
		}

		LOG_DEBUG("Wait for ice nego result at session: " << is.id);		
		is.setStatus(ICE_OP_START_SESSION);		
						
//#pragma message("for test, send a temp ice result to CCS")
//		sendCommand(is, ICE_OP_ICE_RESULT, command.content.c_str(), command.content.size());
	}
		break;
	case ICE_OP_JOIN_SESSION:
	{
		//SessionID sid = genSessionId();
		//assert(sessions_.find(sid) == sessions_.end());
		if (sessions_.find(command.sessionId) != sessions_.end())
		{
			LOG_INFO("This session with id: " << command.sessionId << " is already existed!");
			break;
		}

		readJsonInfo(command.content);

		IceSession& is = sessions_[command.sessionId];
		is.setStatus(ICE_OP_JOIN_SESSION);
		is.id = command.sessionId;
		is.request = sender;	
		int content_len = 0;		
		// reuse send buffer
		if (eice_new_callee(iceJsonConfig(), command.content.data(), command.content.size(), sendBuffer_ + ICE_COMMAND_HEADER_LEN, &content_len, &is.endpoint) != 0)
		{
			LOG_WARN("something wrong with new ice endpoint !!!");
			break;
		}

		// tell sender local info
		sendCommand(is, ICE_OP_LOCAL_INFO, NULL, content_len);

		/// to wait ice session result.
		is.setStatus(ICE_OP_JOIN_SESSION);
	}
	break;
	case ICE_OP_RELAY_SESSION:
	{
		IceSessionMap::iterator it = sessions_.find(command.sessionId);
		if (it == sessions_.end()) {
			LOG_WARN("Received a START_SESSION with inexistent sessionId:" << command.sessionId);
			break;
		}
		IceSession& is = it->second;

		if (!checkStatus((ICE_OP)is.curStatus, ICE_OP_RELAY_SESSION))
			break;

		if (sender != is.request) {
			LOG_WARN("A session should come from the same conference control server.");
		}

		// ice is end, turn to media forward service
		if (command.content.empty()) {
			LOG_WARN("session num" << sessions_.size() << "; Need answer info in ICE_OP_RELAY_SESSION.");
			break;
		}

		std::vector<AddrPair> addr_pairs;
		get_addr_pairs_from_result(is.endpoint, is.iceResult, addr_pairs);
		if (addr_pairs.empty()) {
			LOG_WARN("session num" << sessions_.size() << "; Ice nego is failed for no addr pair.");
			break;
		}
		eice_free(is.endpoint);
		is.endpoint = NULL;

		try
		{
			Json_em::Reader json_reader;
			Json_em::Value addrInfo;
			if (!json_reader.parse(command.content, addrInfo)) {
				LOG_WARN("parse received relay info fail!!!");
				break;
			}
			if (addrInfo["result"].asInt() != 0) {
				LOG_INFO("fail to get webrtc answer.");
				break;
			}
	#pragma  message("how about IPV6")
			// get webrtc peer address from conference_control_server
			sockaddr_in webrtcAddr;			
			webrtcAddr.sin_family = AF_INET;
			if (addrInfo.isMember("audio")) {
				webrtcAddr.sin_port = htons(addrInfo["audio"]["port"].asUInt());
				webrtcAddr.sin_addr.s_addr = inet_addr(addrInfo["audio"]["ip"].asString().data());

				// for forward audio/video
				// 为了测试，写死remote地址
				//addr_pairs[0].remote_ip = "172.17.2.51";			
				//addr_pairs[0].remote_port = 10000;
				g_app.forwardService()->startForward(MEDIA_AUDIO, addr_pairs[0], webrtcAddr);
			}
			else {
				webrtcAddr.sin_port = htons(addrInfo["video"]["port"].asUInt());
				webrtcAddr.sin_addr.s_addr = inet_addr(addrInfo["video"]["ip"].asString().data());

				g_app.forwardService()->startForward(MEDIA_VIDEO, addr_pairs[0], webrtcAddr);
			}
			// 每一对后面一对（1/3）是用作rtcp的，忽略掉
			if (addr_pairs.size() > 2) {
				webrtcAddr.sin_port = htons(addrInfo["video"]["port"].asUInt());
				webrtcAddr.sin_addr.s_addr = inet_addr(addrInfo["video"]["ip"].asString().data());

				//addr_pairs[2].remote_ip = "172.17.2.51";				
				//addr_pairs[2].remote_port = 10002;
				g_app.forwardService()->startForward(MEDIA_VIDEO, addr_pairs[2], webrtcAddr);
			}
		}
		catch (...)
		{
			LOG_INFO("Received error message.");
		}

		/// ice is end, clear ice info
		sessions_.erase(it);
	}
		break;
	case ICE_OP_STOP_SESSION:
	{
		IceSessionMap::iterator it = sessions_.find(command.sessionId);
		if (it == sessions_.end()) {
			LOG_WARN("Received a START_SESSION with inexistent sessionId:" << command.sessionId);
			break;
		}
		IceSession& is = it->second;

		if (is.curStatus > ICE_OP_ICE_RESULT)
			sessions_.erase(it);
		else
			// 如果还在等ice结果，不能删； 如果超时了，但是还没有返回ice结果呢？
			is.curStatus = ICE_OP_END;
	}
	break;
	default:
		LOG_WARN("Unknown command: " << command.op << " content: " << command.content);
		break;
	}
}

IceSession* IceService::findSesssionByEp(eice_t ep)
{	
	for (IceSessionMap::iterator it = sessions_.begin(); it != sessions_.end(); ++it)
	{
		if (it->second.endpoint == ep) {
			return &it->second;
		}
	}
	return NULL;
}

void IceService::timeout_cb(evutil_socket_t fd, short event, void *arg)
{
	assert(arg != NULL);
	IceService* self = (IceService*)arg;

	time_t curTime = time(NULL);
	for (IceSessionMap::iterator it = self->sessions_.begin(); it != self->sessions_.end(); ++it)
	{
		if (((it->second.curStatus == ICE_OP_END) && (curTime - it->second.curTime > SESSION_ALIVE_CHECK_TIME)) ||
			((curTime - it->second.curTime) > SESSION_ALIVE_CHECK_TIME*2))
		{
			LOG_WARN("Session: " << it->second.id << " is timeout for " << curTime - it->second.curTime << "s, remove it!");
			self->sessions_.erase(it);
			return;
		}
	}
}

void IceService::getNegoResult(eice_t obj)
{	
	/// save address
	IceSession* session = findSesssionByEp(obj);
	//assert(findRet);
	if (session == NULL) {
		LOG_WARN("ice obj is not existed!");
		return;
	}
		
	int ret = eice_get_nego_result(obj, session->iceResult, &session->iceResultLen);
	assert(ret == 0 && session->iceResultLen > 0);
	LOG_INFO("+++++++++get caller nego result OK, this result will use as local address at webrtc offer.");
	LOG_INFO(session->iceResult);
	
	// tell conference control server (request)	
	assert(session->request != NULL);
	if (!g_app.iceServer()->haveConnection(session->request)) {
		LOG_INFO("The request have exit, do nothing.");
		return;
	}

	sendCommand(*session, ICE_OP_ICE_RESULT, session->iceResult, session->iceResultLen);	
}