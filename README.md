# bridgeserver
bridge for old xmpp client and new webrtc client

# 编译说明：
 1.  git clone https://github.com/shuinan/bridgeserver.git 
 2.  cd bridgeserver
 3.  修改Makefile中pj路径（第一行）， 
 4.  make
  
step3: bridgeserver依赖带eice的pj库 

另外，项目单元测试依赖cppunit、日志使用了log4cplus，在ubuntu14上直接apt-get install libcppunit-dev liblog4cplus-dev即可。