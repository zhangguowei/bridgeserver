# bridgeserver
bridge for old xmpp client and new webrtc client

# 编译说明：
 1.  git clone https://github.com/shuinan/bridgeserver.git
 2.  ln -s XXX/pj  pj							// bridgeserver依赖带eice的pj库， 编译之前要确保之前的eice可以编译通过
 3.  cd bridgeserver
 4.  cp third/eice/* ../pj/eice/eice/src/		// 本项目对eice做了一些修改
 5.  make
  
