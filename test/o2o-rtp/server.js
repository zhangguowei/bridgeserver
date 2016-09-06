/*
 * (C) Copyright 2014 Kurento (http://kurento.org/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

var path = require('path');
var express = require('express');
var ws = require('ws');
var minimist = require('minimist');
var url = require('url');
var kurento = require('kurento-client');
var fs    = require('fs');
var https = require('https');
var sdpTransform = require('sdp-transform');
var net = require('net');

var argv = minimist(process.argv.slice(2), {
  default: {
      as_uri: "https://localhost:8443/",
      ws_uri: "ws://localhost:8888/kurento"
      //ws_uri: "ws://121.41.87.159:8888/kurento"
  }
});

console.log('process.argv is: ' + process.argv);
console.log('argv.ws_uri is: ' + argv.ws_uri);

var options =
{
  key:  fs.readFileSync('keys/server.key'),
  cert: fs.readFileSync('keys/server.crt')
};

var app = express();

/*
 * Definition of global variables.
 */

var kurentoClient = null;
var userRegistry = new UserRegistry();
var pipelines = {};
var candidatesQueue = {};
var idCounter = 0;

function nextUniqueId() {
    idCounter++;
    return idCounter.toString();
}



// var HOST = '127.0.0.1';
// var PORT = 6969;
// var ccs_client = new net.Socket();
// ccs_client.connect(PORT, HOST, function() {
//     console.log('CONNECTED TO: ' + HOST + ':' + PORT);
//     // 建立连接后立即向服务器发送数据，服务器将收到这些数据
//     //ccs_client.write('I am Chuck Norris!');
// });
// // 为客户端添加“data”事件处理函数 // data是服务器发回的数据
// ccs_client.on('data', function(data) {
//     console.log('DATA: ' + data);
//     // 完全关闭连接
//     //ccs_client.destroy();
// });
// // 为客户端添加“close”事件处理函数
// ccs_client.on('close', function() {
//     console.log('Connection closed');
// });


/*
 * Definition of helper classes
 */

// Represents caller and callee sessions
function UserSession(id, name, mode, ws) {
    var me = this;

    this.id = id;
    this.name = name;
    this.ws = ws;
    this.mode = mode;
    this.peer = null;
    this.sdpOffer = null;
    
    this.isWebrtc = function () {
        return me.mode === 'webrtc'
    }
    this.endPointName = function () {
        return (me.mode === 'webrtc') ? 'WebRtcEndpoint' : 'RtpEndpoint';
    }
}

UserSession.prototype.sendMessage = function(message) {
    this.ws.send(JSON.stringify(message));
}

// Represents registrar of users
function UserRegistry() {
    this.usersById = {};
    this.usersByName = {};
}

UserRegistry.prototype.register = function(user) {
    this.usersById[user.id] = user;
    this.usersByName[user.name] = user;
}

UserRegistry.prototype.unregister = function(id) {
    var user = this.getById(id);
    if (user) delete this.usersById[id]
    if (user && this.getByName(user.name)) delete this.usersByName[user.name];
}

UserRegistry.prototype.getById = function(id) {
    return this.usersById[id];
}

UserRegistry.prototype.getByName = function(name) {
    return this.usersByName[name];
}

UserRegistry.prototype.removeById = function(id) {
    var userSession = this.usersById[id];
    if (!userSession) return;
    delete this.usersById[id];
    delete this.usersByName[userSession.name];
}

// Represents a B2B active call
function CallMediaPipeline() {
    this.pipeline = null;
    this.webRtcEndpoint = {};
}

CallMediaPipeline.prototype.createPipeline = function(callerId, calleeId, ws, callback) {
    var self = this;
    getKurentoClient(function(error, kurentoClient) {
        if (error) {
            return callback(error);
        }

        kurentoClient.create('MediaPipeline', function(error, pipeline) {
            if (error) {
                return callback(error);
            }

            // filter candidate, remove tcp type, only use audio/video
            // only use unique candidate
            function filterCandidate(webRtcEndpoint, candidate) {
                if (candidate.candidate.toLowerCase().indexOf('udp') == -1) {
                    console.log("KMS ice candidate is not udp type, no use it.", candidate);
                    return true;
                }

                webRtcEndpoint.candidates = webRtcEndpoint.candidates || [];
                var curQueue = webRtcEndpoint.candidates;
                for(var i = 0; i < curQueue.length; ++i) {
                    if (curQueue[i].candidate == candidate.candidate) {
                        console.log("KMS have same candidate info.", candidate);
                        return true;
                    }
                }
                webRtcEndpoint.candidates.push(candidate);

                return false;
            }


            var caller = userRegistry.getById(callerId);
            var callee = userRegistry.getById(calleeId);
	        pipeline.create(caller.endPointName(), function(error, callerWebRtcEndpoint) {
                if (error) {
                    pipeline.release();
                    return callback(error);
                }

                if (caller.isWebrtc()) {
                    if (candidatesQueue[callerId]) {
                        while(candidatesQueue[callerId].length) {
                            var candidate = candidatesQueue[callerId].shift();
                            callerWebRtcEndpoint.addIceCandidate(candidate);
                        }
                    }

                    callerWebRtcEndpoint.on('OnIceCandidate', function (event) {
                        var candidate = kurento.getComplexType('IceCandidate')(event.candidate);

                        if (filterCandidate(callerWebRtcEndpoint, candidate))
                            return;

                        userRegistry.getById(callerId).ws.send(JSON.stringify({
                            id: 'iceCandidate',
                            candidate: candidate
                        }));
                    });
                }

                pipeline.create(callee.endPointName(), function(error, calleeWebRtcEndpoint) {
                    if (error) {
                        pipeline.release();
                        return callback(error);
                    }

                    if (callee.isWebrtc()) {
                        if (candidatesQueue[calleeId]) {
                            while(candidatesQueue[calleeId].length) {
                                var candidate = candidatesQueue[calleeId].shift();
                                calleeWebRtcEndpoint.addIceCandidate(candidate);
                            }
                        }

                        calleeWebRtcEndpoint.on('OnIceCandidate', function (event) {
                            var candidate = kurento.getComplexType('IceCandidate')(event.candidate);

                            if (filterCandidate(calleeWebRtcEndpoint, candidate))
                                return;

                            userRegistry.getById(calleeId).ws.send(JSON.stringify({
                                id: 'iceCandidate',
                                candidate: candidate
                            }));
                        });
                    }

                    callerWebRtcEndpoint.connect(calleeWebRtcEndpoint, function(error) {
                        if (error) {
                            pipeline.release();
                            return callback(error);
                        }

                        calleeWebRtcEndpoint.connect(callerWebRtcEndpoint, function(error) {
                            if (error) {
                                pipeline.release();
                                return callback(error);
                            }
                        });

                        self.pipeline = pipeline;
                        self.webRtcEndpoint[callerId] = callerWebRtcEndpoint;
                        self.webRtcEndpoint[calleeId] = calleeWebRtcEndpoint;
                        callback(null);
                    });
                });
            });
        });
    })
}

CallMediaPipeline.prototype.generateSdpAnswer = function(id, sdpOffer, needIce, callback) {
    this.webRtcEndpoint[id].processOffer(sdpOffer, callback);
    if (needIce) {
        this.webRtcEndpoint[id].gatherCandidates(function (error) {
            if (error) {
                return callback(error);
            }
        });
    }
}

CallMediaPipeline.prototype.release = function() {
    if (this.pipeline) this.pipeline.release();
    this.pipeline = null;
}

/*
 * Server startup
 */

var asUrl = url.parse(argv.as_uri);
var port = asUrl.port;
var server = https.createServer(options, app).listen(port, function() {
    console.log('Kurento Tutorial started');
    console.log('Open ' + url.format(asUrl) + ' with a WebRTC capable browser');
});

var wss = new ws.Server({
    server : server,
    path : '/one2one'
});

wss.on('connection', function(ws) {
    var sessionId = nextUniqueId();
    console.log('Connection received with sessionId ' + sessionId);

    ws.on('error', function(error) {
        console.log('Connection ' + sessionId + ' error');
        stop(sessionId);
    });

    ws.on('close', function() {
        console.log('Connection ' + sessionId + ' closed');
        stop(sessionId);
        userRegistry.unregister(sessionId);
    });

    ws.on('message', function(_message) {
        var message = JSON.parse(_message);
        console.log('Connection ' + sessionId + ' received message ', message);

        switch (message.id) {
        case 'register':
            register(sessionId, message.name, message.mode, ws);
            break;

        case 'call':
            call(sessionId, message.to, message.from, message.sdpOffer);
            break;

        case 'incomingCallResponse':
            incomingCallResponse(sessionId, message.from, message.callResponse, message.sdpOffer, ws);
            break;

        case 'stop':
            stop(sessionId);
            break;

        case 'onIceCandidate':
            onIceCandidate(sessionId, message.candidate);
            break;

        default:
            ws.send(JSON.stringify({
                id : 'error',
                message : 'Invalid message ' + message
            }));
            break;
        }

    });
});

// Recover kurentoClient for the first time.
function getKurentoClient(callback) {
    if (kurentoClient !== null) {
        return callback(null, kurentoClient);
    }

    kurento(argv.ws_uri, function(error, _kurentoClient) {
        if (error) {
            var message = 'Coult not find media server at address ' + argv.ws_uri;
            return callback(message + ". Exiting with error " + error);
        }

        kurentoClient = _kurentoClient;
        callback(null, kurentoClient);
    });
}

function stop(sessionId) {
    if (!pipelines[sessionId]) {
        return;
    }

    var pipeline = pipelines[sessionId];
    delete pipelines[sessionId];
    pipeline.release();
    var stopperUser = userRegistry.getById(sessionId);
    var stoppedUser = userRegistry.getByName(stopperUser.peer);
    stopperUser.peer = null;

    if (stoppedUser) {
        stoppedUser.peer = null;
        delete pipelines[stoppedUser.id];
        var message = {
            id: 'stopCommunication',
            message: 'remote user hanged out'
        }
        stoppedUser.sendMessage(message)
    }

    clearCandidatesQueue(sessionId);
}
var sessionId = 0;
function incomingCallResponse(calleeId, from, callResponse, calleeSdp, ws) {

    clearCandidatesQueue(calleeId);

    function onError(callerReason, calleeReason) {
        if (pipeline) pipeline.release();
        if (caller) {
            var callerMessage = {
                id: 'callResponse',
                response: 'rejected'
            }
            if (callerReason) callerMessage.message = callerReason;
            caller.sendMessage(callerMessage);
        }

        var calleeMessage = {
            id: 'stopCommunication'
        };
        if (calleeReason) calleeMessage.message = calleeReason;
        callee.sendMessage(calleeMessage);
    }

    var callee = userRegistry.getById(calleeId);
    callee.sdpOffer = calleeSdp;
    if (!from || !userRegistry.getByName(from)) {
        return onError(null, 'unknown from = ' + from);
    }
    var caller = userRegistry.getByName(from);

    if (callResponse === 'accept') {
        var pipeline = new CallMediaPipeline();
        pipelines[caller.id] = pipeline;
        pipelines[callee.id] = pipeline;

        pipeline.createPipeline(caller.id, callee.id, ws, function(error) {
            if (error) {
                return onError(error, error);
            }

            function getSdpOffer(user) {
                /// without rtcp:
                //var sdpOffer = 'v=0\r\no=- 0 0 IN IP4 172.17.2.51\r\ns=-\r\nt=0 0\r\na=msid-semantic: WMS\r\nm=audio 10000 RTP/AVP 120 103 104 9 0 8 106 105 13 126\r\nc=IN IP4 172.17.2.51\r\na=mid:audio\r\na=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\na=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\na=recvonly\r\na=rtpmap:120 opus/48000/2\r\na=fmtp:111 minptime=10;useinbandfec=1\r\na=rtpmap:103 ISAC/16000\r\na=rtpmap:104 ISAC/32000\r\na=rtpmap:9 G722/8000\r\na=rtpmap:0 PCMU/8000\r\na=rtpmap:8 PCMA/8000\r\na=rtpmap:106 CN/32000\r\na=rtpmap:105 CN/16000\r\na=rtpmap:13 CN/8000\r\na=rtpmap:126 telephone-event/8000\r\na=maxptime:60\r\nm=video 10002 RTP/AVP 100 101 116 117 96 97 98\r\nc=IN IP4 172.17.2.51\r\na=mid:video\r\na=extmap:2 urn:ietf:params:rtp-hdrext:toffset\r\na=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\na=extmap:4 urn:3gpp:video-orientation\r\na=recvonly\r\na=rtpmap:100 VP8/90000\r\na=rtpmap:101 VP9/90000\r\na=rtpmap:116 red/90000\r\na=rtpmap:117 ulpfec/90000\r\na=rtpmap:96 vp8/90000\r\na=fmtp:96 apt=100\r\na=rtpmap:97 rtx/90000\r\na=fmtp:97 apt=101\r\na=rtpmap:98 rtx/90000\r\na=fmtp:98 apt=116\r\n';
                //var sdpOffer = 'v=0\r\no=- 0 0 IN IP4 172.17.2.51\r\ns=-\r\nt=0 0\r\na=msid-semantic: WMS\r\nm=audio 10000 RTP/AVP 120 103 104 9 0 8 106 105 13 126\r\nc=IN IP4 172.17.2.51\r\na=rtcp:9 IN IP4 172.17.2.51\r\na=mid:audio\r\na=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\na=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\na=recvonly\r\na=rtpmap:120 opus/48000/2\r\na=rtcp-fb:111 transport-cc\r\na=fmtp:111 minptime=10;useinbandfec=1\r\na=rtpmap:103 ISAC/16000\r\na=rtpmap:104 ISAC/32000\r\na=rtpmap:9 G722/8000\r\na=rtpmap:0 PCMU/8000\r\na=rtpmap:8 PCMA/8000\r\na=rtpmap:106 CN/32000\r\na=rtpmap:105 CN/16000\r\na=rtpmap:13 CN/8000\r\na=rtpmap:126 telephone-event/8000\r\na=maxptime:60\r\nm=video 10002 RTP/AVP 100 101 116 117 96 97 98\r\nc=IN IP4 172.17.2.51\r\na=rtcp:9 IN IP4 172.17.2.51\r\na=mid:video\r\na=extmap:2 urn:ietf:params:rtp-hdrext:toffset\r\na=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\na=extmap:4 urn:3gpp:video-orientation\r\na=recvonly\r\na=rtcp-rsize\r\na=rtpmap:100 VP8/90000\r\na=rtcp-fb:100 ccm fir\r\na=rtcp-fb:100 nack\r\na=rtcp-fb:100 nack pli\r\na=rtcp-fb:100 goog-remb\r\na=rtcp-fb:100 transport-cc\r\na=rtpmap:101 VP9/90000\r\na=rtcp-fb:101 ccm fir\r\na=rtcp-fb:101 nack\r\na=rtcp-fb:101 nack pli\r\na=rtcp-fb:101 goog-remb\r\na=rtcp-fb:101 transport-cc\r\na=rtpmap:116 red/90000\r\na=rtpmap:117 ulpfec/90000\r\na=rtpmap:96 vp8/90000\r\na=fmtp:96 apt=100\r\na=rtpmap:97 rtx/90000\r\na=fmtp:97 apt=101\r\na=rtpmap:98 rtx/90000\r\na=fmtp:98 apt=116\r\n';
                // no rtcp
                //var sdpOffer = 'v=0\r\no=- 0 0 IN IP4 172.17.2.51\r\ns=-\r\nt=0 0\r\na=msid-semantic: WMS\r\nm=audio 10000 RTP/AVP 120 103 104 9 0 8 106 105 13 126\r\nc=IN IP4 172.17.2.51\r\na=mid:audio\r\na=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\na=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\na=sendrecv\r\na=rtpmap:120 opus/48000/2\r\na=fmtp:120 minptime=10;useinbandfec=1\r\na=rtpmap:103 ISAC/16000\r\na=rtpmap:104 ISAC/32000\r\na=rtpmap:9 G722/8000\r\na=rtpmap:0 PCMU/8000\r\na=rtpmap:8 PCMA/8000\r\na=rtpmap:106 CN/32000\r\na=rtpmap:105 CN/16000\r\na=rtpmap:13 CN/8000\r\na=rtpmap:126 telephone-event/8000\r\na=maxptime:60\r\nm=video 10002 RTP/AVP 100 101 116 117 96 97 98\r\nc=IN IP4 172.17.2.51\r\na=mid:video\r\na=extmap:2 urn:ietf:params:rtp-hdrext:toffset\r\na=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\na=extmap:4 urn:3gpp:video-orientation\r\na=sendrecv\r\na=rtpmap:100 VP8/90000\r\na=rtpmap:101 VP9/90000\r\na=rtpmap:116 red/90000\r\na=rtpmap:117 ulpfec/90000\r\na=rtpmap:96 vp8/90000\r\na=fmtp:96 apt=100\r\na=rtpmap:97 rtx/90000\r\na=fmtp:97 apt=101\r\na=rtpmap:98 rtx/90000\r\na=fmtp:98 apt=116\r\n';
                // add h264
                var sdpBase = 'v=0\r\no=- 0 0 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\na=msid-semantic: WMS\r\n';
                //var sdpAudio = 'm=audio 10000 RTP/AVP 120 103 104 9 0 8 106 105 13 126\r\nc=IN IP4 172.17.2.51\r\na=mid:audio\r\na=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\na=sendrecv\r\na=rtpmap:120 opus/48000/2\r\na=fmtp:120 maxaveragebitrate=40000;usedtx=0;stereo=0;useinbandfec=0;maxcodedaudiobandwidth=fb;cbr=0\r\na=fmtp:120 minptime=10;useinbandfec=1\r\na=rtpmap:103 ISAC/16000\r\na=rtpmap:104 ISAC/32000\r\na=rtpmap:9 G722/8000\r\na=rtpmap:0 PCMU/8000\r\na=rtpmap:8 PCMA/8000\r\na=rtpmap:106 CN/32000\r\na=rtpmap:105 CN/16000\r\na=rtpmap:13 CN/8000\r\na=rtpmap:126 telephone-event/8000\r\na=maxptime:60\r\n';
                var sdpAudio = 'm=audio 10000 RTP/AVP 120\r\nc=IN IP4 172.17.2.51\r\na=mid:audio\r\na=sendrecv\r\na=rtpmap:120 opus/48000/2\r\na=fmtp:111 minptime=10;useinbandfec=0\r\na=fmtp:120 minptime=10;useinbandfec=0\r\na=maxptime:60\r\n';
                //var sdpVideo = 'm=video 10002 RTP/AVP 96\r\nc=IN IP4 172.17.2.51\r\na=mid:video\r\na=sendrecv\r\na=rtpmap:96 H264/90000\r\na=fmtp:96 profile-level-id=42A01E; packetization-mode=1;sprop-parameter-sets=Z0IACpZTBYmI,aMljiA==\r\n';
                var sdpVideo = 'm=video 9 RTP/AVP 96 100 101 116 117 95 97 98\r\nc=IN IP4 172.17.2.51\r\na=mid:video\r\na=extmap:2 urn:ietf:params:rtp-hdrext:toffset\r\na=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\na=extmap:4 urn:3gpp:video-orientation\r\na=sendrecv\r\na=rtpmap:100 VP8/90000\r\na=rtpmap:101 VP9/90000\r\na=rtpmap:116 red/90000\r\na=rtpmap:117 ulpfec/90000\r\na=rtpmap:95 vp8/90000\r\na=fmtp:95 apt=100\r\na=rtpmap:96 H264/90000\r\na=fmtp:96 profile-level-id=42A01E; packetization-mode=1;sprop-parameter-sets=Z0IACpZTBYmI,aMljiA==\r\na=rtpmap:97 rtx/90000\r\na=fmtp:97 apt=101\r\na=rtpmap:98 rtx/90000\r\na=fmtp:98 apt=116\r\n';
                // only video
                //var sdpOffer = 'v=0\r\no=- 0 0 IN IP4 172.17.2.51\r\ns=-\r\nt=0 0\r\na=msid-semantic: WMS\r\nm=video 10002 RTP/AVP 100 101 116 117 96 97 98\r\nc=IN IP4 172.17.2.51\r\na=mid:video\r\na=extmap:2 urn:ietf:params:rtp-hdrext:toffset\r\na=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\na=extmap:4 urn:3gpp:video-orientation\r\na=sendrecv\r\na=rtpmap:100 VP8/90000\r\na=rtpmap:101 VP9/90000\r\na=rtpmap:116 red/90000\r\na=rtpmap:117 ulpfec/90000\r\na=rtpmap:96 vp8/90000\r\na=fmtp:96 apt=100\r\na=rtpmap:97 rtx/90000\r\na=fmtp:97 apt=101\r\na=rtpmap:98 rtx/90000\r\na=fmtp:98 apt=116\r\n';
               //var sdpOffer = 'v=0\r\no=- 5456647314240798638 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\na=msid-semantic: WMS tuKWNVRQjmhTw64djEz50UbExWTldvyD3R07\r\nm=audio 10000 RTP/AVP 120 103 104 9 0 8 106 105 13 126\r\nc=IN IP4 172.17.2.51\r\na=rtcp:9 IN IP4 0.0.0.0\r\na=mid:audio\r\na=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\na=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\na=sendrecv\r\na=rtcp-mux\r\na=rtpmap:120 opus/48000/2\r\na=rtcp-fb:120 transport-cc\r\na=fmtp:120 minptime=10;useinbandfec=1\r\na=rtpmap:103 ISAC/16000\r\na=rtpmap:104 ISAC/32000\r\na=rtpmap:9 G722/8000\r\na=rtpmap:0 PCMU/8000\r\na=rtpmap:8 PCMA/8000\r\na=rtpmap:106 CN/32000\r\na=rtpmap:105 CN/16000\r\na=rtpmap:13 CN/8000\r\na=rtpmap:126 telephone-event/8000\r\na=maxptime:60\r\nm=video 10002 RTP/AVP 96 100 101 116 117 99 97 98\r\nc=IN IP4 172.17.2.51\r\na=rtcp:9 IN IP4 0.0.0.0\r\na=mid:video\r\na=extmap:2 urn:ietf:params:rtp-hdrext:toffset\r\na=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\na=extmap:4 urn:3gpp:video-orientation\r\na=sendrecv\r\na=rtcp-mux\r\na=rtcp-rsize\r\na=rtpmap:100 VP8/90000\r\na=rtcp-fb:100 ccm fir\r\na=rtcp-fb:100 nack\r\na=rtcp-fb:100 nack pli\r\na=rtcp-fb:100 goog-remb\r\na=rtcp-fb:100 transport-cc\r\na=rtpmap:101 VP9/90000\r\na=rtcp-fb:101 ccm fir\r\na=rtcp-fb:101 nack\r\na=rtcp-fb:101 nack pli\r\na=rtcp-fb:101 goog-remb\r\na=rtcp-fb:101 transport-cc\r\na=rtpmap:116 red/90000\r\na=rtpmap:117 ulpfec/90000\r\na=rtpmap:96 H264/90000\r\na=fmtp:96 profile-level-id=42A01E; packetization-mode=1;sprop-parameter-sets=Z0IACpZTBYmI,aMljiA==\r\na=rtpmap:99 rtx/90000\r\na=fmtp:99 apt=100\r\na=rtpmap:97 rtx/90000\r\na=fmtp:97 apt=101\r\na=rtpmap:98 rtx/90000\r\na=fmtp:98 apt=116\r\na=ssrc-group:FID 2814735253 3222904659\r\n';
                // no rtcp
                var sdpOffer = 'v=0\r\no=- 5456647314240798638 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\na=msid-semantic: WMS tuKWNVRQjmhTw64djEz50UbExWTldvyD3R07\r\nm=audio 10000 RTP/AVP 120\r\nc=IN IP4 172.17.3.7\r\na=mid:audio\r\na=sendrecv\r\na=rtpmap:120 opus/48000/2\r\na=fmtp:120 maxaveragebitrate=40000;usedtx=0;stereo=1;minptime=20;useinbandfec=0;cbr=0\r\na=maxptime:100\r\nm=video 10002 RTP/AVP 96\r\nc=IN IP4 172.17.3.7\r\na=mid:video\r\na=sendrecv\r\na=rtpmap:96 H264/90000\r\na=fmtp:96 profile-level-id=42800d; packetization-mode=1;sprop-parameter-sets=sprop-parameter-sets=Z2QAKKy0Cg/QgAAAAwCAAAApR4wZUA==,aO88sA==\r\n';

                //var sdpOffer = sdpBase + sdpAudio + sdpVideo;

                //var webrtcOffer_json = sdpTransform.parse(user.sdpOffer);
                //webrtcOffer_json.media[1].payloads = '107 100 101 116 117 96 97 99 98'
                //var webrtcOffer = sdpTransform.write(webrtcOffer_json);
                var webrtcOffer = user.sdpOffer.replace('100 101 107', '107 100 101');
                //console.log("+++++++++ modified sdp: " + webrtcOffer);
                console.assert(!!user.sdpOffer, user.name+" user.offer should not be empty.");
                //return user.isWebrtc() ? user.sdpOffer : sdpOffer;
                return user.isWebrtc() ? webrtcOffer : sdpOffer;
            }
            pipeline.generateSdpAnswer(caller.id, getSdpOffer(caller), caller.isWebrtc(), function(error, callerSdpAnswer) {
                if (error) {
                    return onError(error, error);
                }

                console.log("+++++++++ Caller offer: " + getSdpOffer(caller));
                console.log("+++++++++ Caller answer: " + callerSdpAnswer);

                var count = 0;
                var t1 = setInterval(function() {
                    ++count;
                    console.log("come to here 97-----------------------");
                    fs.readFile('/home/shangrong/iceAddr.json',function(err, data) {
                        if (err) {
                            if (count > 30)
                                clearInterval(t1);
                            return;
                        }
                        console.log("come to here 98-----------------------");
                        var jsonObj;
                        try{
                            jsonObj = JSON.parse(data);
                            console.log("iceaddr.json: " + data);
                            console.log(jsonObj);
                            if (!jsonObj.pairs || jsonObj.pairs.length < 2) {
                                console.log("should have 2 address at least.");
                                return;
                            }
                        } catch(exception) {
                            console.log("Error " + exception);
                            if (count > 30) clearInterval(t1);
                            return;
                        }

                        var offer = getSdpOffer(callee);
                        offer = offer.replace(10000, jsonObj.pairs[0].local.port);
                        if (jsonObj.pairs.length > 2)
                            offer = offer.replace(10002, jsonObj.pairs[2].local.port);
                        else if (jsonObj.pairs.length > 1)
                            offer = offer.replace(10002, jsonObj.pairs[1].local.port);
                        console.log("come to here 99-----------------------");
                        pipeline.generateSdpAnswer(callee.id, offer, callee.isWebrtc(), function(error, calleeSdpAnswer) {
                            if (error) {
                                return onError(error, error);
                            }

                            // send answer to ccs
                            console.log("come to here 100-----------------------");
                            var res = sdpTransform.parse(calleeSdpAnswer);
                            var out_answer = {'audio': {'ip': res.origin.address, 'port': res.media[0].port }, 'video': {'ip': res.origin.address, 'port': res.media[1].port }, 'result': 0 }
                            console.log("write info: " + out_answer);
                            fs.writeFile('/home/shangrong/answer.ini', "webrtc_answer =" + JSON.stringify(out_answer)+"\n", function(err){
                                if(err) throw err;
                                console.log('write JSON into TEXT');
                            });


                            console.log("+++++++++ Callee offer: " + offer);
                            console.log("+++++++++ Callee answer: " + calleeSdpAnswer);

                            var message = {
                                id: 'startCommunication',
                                sdpAnswer: calleeSdpAnswer
                            };
                            callee.sendMessage(message);

                            message = {
                                id: 'callResponse',
                                response : 'accepted',
                                sdpAnswer: callerSdpAnswer
                            };
                            caller.sendMessage(message);
                        });

                        clearInterval(t1);
                    });
                }, 1000);

            });
        });
    } else {
        var decline = {
            id: 'callResponse',
            response: 'rejected',
            message: 'user declined'
        };
        caller.sendMessage(decline);
    }
}

function call(callerId, to, from, sdpOffer) {
    clearCandidatesQueue(callerId);

    var caller = userRegistry.getById(callerId);
    var rejectCause = 'User ' + to + ' is not registered';
    if (userRegistry.getByName(to)) {
        var callee = userRegistry.getByName(to);
        caller.sdpOffer = sdpOffer
        callee.peer = from;
        caller.peer = to;
        var message = {
            id: 'incomingCall',
            from: from
        };
        try{
            return callee.sendMessage(message);
        } catch(exception) {
            rejectCause = "Error " + exception;
        }
    }
    var message  = {
        id: 'callResponse',
        response: 'rejected: ',
        message: rejectCause
    };
    caller.sendMessage(message);
}

function register(id, name, mode, ws, callback) {
    function onError(error) {
        ws.send(JSON.stringify({id:'registerResponse', response : 'rejected ', message: error}));
    }

    if (!name) {
        return onError("empty user name");
    }

    if (userRegistry.getByName(name)) {
        return onError("User " + name + " is already registered");
    }

    userRegistry.register(new UserSession(id, name, mode, ws));
    try {
        ws.send(JSON.stringify({id: 'registerResponse', response: 'accepted'}));
    } catch(exception) {
        onError(exception);
    }
}

function clearCandidatesQueue(sessionId) {
    if (candidatesQueue[sessionId]) {
        delete candidatesQueue[sessionId];
    }
}

function onIceCandidate(sessionId, _candidate) {
    var candidate = kurento.getComplexType('IceCandidate')(_candidate);
    var user = userRegistry.getById(sessionId);

    if (!user.isWebrtc()) {
        return;
    }

    console.log("--------peer candidate: ", candidate);

    // filter candidate, remove tcp type, only use audio/video
    if (candidate.candidate.toLowerCase().indexOf('udp') == -1) {
        console.log("Peer ice candidate is not udp type, no use it.", candidate);
        return;
    }

    if (pipelines[user.id] && pipelines[user.id].webRtcEndpoint && pipelines[user.id].webRtcEndpoint[user.id]) {
        var webRtcEndpoint = pipelines[user.id].webRtcEndpoint[user.id];
        webRtcEndpoint.addIceCandidate(candidate);
    }
    else {
        if (!candidatesQueue[user.id]) {
            candidatesQueue[user.id] = [];
        }

        // only use unique candidate
        var curQueue = candidatesQueue[sessionId];
        for(var i = 0; i < curQueue.length; ++i) {
            if (curQueue[i].candidate == candidate.candidate) {
                console.log("Peer have same candidate info.", candidate);
                return;
            }
        }

        candidatesQueue[sessionId].push(candidate);
    }
}

app.use(express.static(path.join(__dirname, 'static')));
