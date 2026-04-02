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
#define MAX_EVENTS 100 // 定义最大监听连接数量
#define READ_BUF 1024
void setnonblocking(int fd)
{
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

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
    Socket* serv_sock = new Socket();
    InetAddress* serv_addr = new InetAddress("0.0.0.0", 8888);
    // 绑定socket和文件描述符
    serv_sock->bind(serv_addr);
    // 需要监听这个创建出来的套接字
    serv_sock->listen();

    int epfd = epoll_create1(0); // 创建一个用于在内核存放数据的句柄
    errif(epfd == -1, "epoll create error");
    struct epoll_event events[MAX_EVENTS], ev; // events是存放事件的数组，ev是存放事件的模板
    bzero(&events, sizeof(events));
    bzero(&ev, sizeof(ev));
    ev.events = EPOLLIN | EPOLLET; // ET模式,接收连接最好不要使用ET
    ev.data.fd = serv_sock->getfd();           // 将IO口置为服务器的socket fd
    setnonblocking(serv_sock->getfd());
    epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sock->getfd(), &ev); // 将服务器socket fd添加到epoll
    while (true)
    {
        // 返回就绪事件的数量
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1); // 一直阻塞等待事件发生返回
        errif(nfds == -1, "epoll wait error");
        for (int i = 0; i < nfds; ++i)
        {
            if (events[i].data.fd == serv_sock->getfd()) // 发生事件的是服务器socket fd，表示新客户端连接
            {
                // 定义一个sockaddr_in的变量用来接收客户端的socket请求
                struct sockaddr_in clnt_addr;
                socklen_t clnt_addr_len = sizeof(clnt_addr);
                bzero(&clnt_addr, sizeof(clnt_addr));
                // accept阻塞当前进程，用以接收客服端的socket连接
                int clnt_sockfd = accept(serv_sock->getfd(), (sockaddr *)&clnt_addr, &clnt_addr_len);
                errif(clnt_sockfd == -1, "socket accept error");
                printf("new client fd %d! IP: %s Port: %d\n",
                       clnt_sockfd, inet_ntoa(clnt_addr.sin_addr), ntohs(clnt_addr.sin_port));
                bzero(&ev, sizeof(ev));
                ev.data.fd = clnt_sockfd;                         // 设置样板参数
                ev.events = EPOLLIN | EPOLLET;                    // 客户端使用ET模式,让epoll更加高效，支持更多并发
                setnonblocking(clnt_sockfd);                      // ET需要非阻塞socket配合
                epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sockfd, &ev); // 将客户端socket fd添加到epoll
            }
            else if (events[i].events & EPOLLIN) // 发生事件的是客户端，并且是可读事件(EPOLLIN)
            {
                handleEvent(events[i].data.fd); // 处理fd发生事件
            }
            else
            {
                printf("something else happend\n");
            }
        }
    }
    close(sockfd);
    return 0;
}
