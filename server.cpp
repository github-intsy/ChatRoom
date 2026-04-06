#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <sys/epoll.h>
#include "util.h"
#include "Socket.h"
#include "InetAddress.h"
#include "Channel.h"
#include "Epoll.h"
#define MAX_EVENTS 100
#define READ_BUF 1024

// 服务器处理事项,非阻塞io需要不断读取，一次事件读取完毕
void handleEvent(int clnt_sockfd)
{
    char buf[READ_BUF]; // 定义读取缓冲区
    // 循环读取数据
    while (true)
    {
        bzero(&buf, sizeof(buf)); // 清空缓冲区
        // 从客户端socketfd读取书数据到缓冲区，返回已读取数据大小
        ssize_t read_bytes = read(clnt_sockfd, buf, sizeof(buf));
        if (read_bytes > 0)
        {
            printf("message from client fd %d: %s\n", clnt_sockfd, buf);
            write(clnt_sockfd, buf, sizeof(buf)); // 将获取到的数据写回给客户端
        }
        else if (read_bytes == 0) // read返回0，表示客户端关闭连接，EOF
        {
            printf("client fd %d disconnected\n", clnt_sockfd);
            close(clnt_sockfd);
            break;
        }
        else if (read_bytes == -1 && errno == EINTR) // 客户端正常中断，继续读取
        {
            printf("continue reading");
            continue;
        }
        // 非阻塞IO，这个条件表示数据全部读取完毕
        else if (read_bytes == -1 && ((errno == EAGAIN) || errno == EWOULDBLOCK))
        {
            printf("finish reading once, errno: %d\n", errno);
            break;
        }
    }
}
int main()
{
    Socket *serv_sock = new Socket();
    InetAddress *serv_addr = new InetAddress("0.0.0.0", 8888);
    // 绑定socket和文件描述符
    serv_sock->bind(serv_addr);
    // 需要监听这个创建出来的套接字
    serv_sock->listen();
    // ET需要非阻塞socket配合
    setnonblocking(serv_sock->getfd());
    Epoll *ep = new Epoll();
    Channel *servChannel = new Channel(ep, serv_sock->getfd());
    servChannel->enableReading();
    while (true)
    {
        // 返回就绪事件的数量
        std::vector<Channel *> activeChannels = ep->poll();
        for (int i = 0; i < activeChannels.size(); ++i)
        {
            int chfd = activeChannels[i]->getFd();
            if (chfd == serv_sock->getfd()) // 发生事件的是服务器socket fd，表示新客户端连接
            {
                // 定义一个sockaddr_in的变量用来接收客户端的socket请求
                InetAddress *clnt_addr = new InetAddress();
                Socket *clnt_sock = new Socket(serv_sock->accept(clnt_addr));
                // accept阻塞当前进程，用以接收客服端的socket连接
                errif(clnt_sock->getfd() == -1, "socket accept error");
                printf("new client fd %d! IP: %s Port: %d\n",
                       clnt_sock->getfd(), inet_ntoa(clnt_addr->_addr.sin_addr), ntohs(clnt_addr->_addr.sin_port));
                setnonblocking(clnt_sock->getfd());
                Channel *clntChannel = new Channel(ep, clnt_sock->getfd());
                clntChannel->enableReading();
                // 客户端使用ET模式,让epoll更加高效，支持更多并发
            }
            else if (activeChannels[i]->getEvents() & EPOLLIN) // 发生事件的是客户端，并且是可读事件(EPOLLIN)
            {
                handleEvent(activeChannels[i]->getFd()); // 处理fd发生事件
            }
            else
            {
                printf("something else happend\n");
            }
        }
    }
    delete serv_sock;
    delete serv_addr;
    return 0;
}
