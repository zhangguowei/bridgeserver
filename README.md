# bridgeserver
bridge for old xmpp client and new webrtc client

# 编译说明：
 1.  git clone https://github.com/shuinan/bridgeserver.git 
 2.  cd bridgeserver
 3.  make  [PJ_PATH=xxx]
  
step3: bridgeserver依赖带eice的pj库, xxx 是pj所在的目录，缺省认为和bridgeserver同级

另外，项目单元测试依赖：
	* libevent-2.0以上
	* 单元测试cppunit
	* 日志log4cplus
在ubuntu14上直接apt-get install libcppunit-dev liblog4cplus-dev即可。libevent可以下载代码安装
