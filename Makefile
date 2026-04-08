server:
	g++ src/Connection.cpp src/Acceptor.cpp src/util.cpp server.cpp src/Socket.cpp src/Buffer.cpp \
	src/Epoll.cpp src/InetAddress.cpp src/Channel.cpp src/EventLoop.cpp src/Server.cpp -o server  && \
	g++ src/util.cpp client.cpp -o client
clean:
	rm -rf server &&\
	rm -rf client