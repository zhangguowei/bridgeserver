# bridgeserver
bridge for old xmpp client and new webrtc client

# 编译说明：
 1.  git clone https://github.com/shuinan/bridgeserver.git
 2.  ln -s XXX/pj  pj							
 3.  cd bridgeserver
 4.  cp third/eice/* ../pj/eice/eice/src/		
 5.  make
  
step2: bridgeserver依赖带eice的pj库， 编译之前要确保之前的eice可以编译通过, 其中**“XXX”**是pj库所在的路径
step4: 本项目对eice做了一些修改; 执行这步时，一定要注意会**覆盖**原来的文件（eice.h和eice.cpp），要对原来的文件做好**备份**

另外，项目单元测试依赖cppunit、日志使用了log4cplus，在ubuntu14上直接apt-get install libcppunit-dev liblog4cplus-dev即可。