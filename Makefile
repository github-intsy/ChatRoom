server:
	g++ util.cpp server.cpp Socket.cpp Epoll.cpp InetAddress.cpp Channel.cpp -o server  && \
	g++ util.cpp client.cpp Socket.cpp Epoll.cpp InetAddress.cpp Channel.cpp -o client
clean:
	rm -rf server &&\
	rm -rf client