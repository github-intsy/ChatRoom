#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <cstdio>
#include "src/util.h"
int main()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(8888);
    errif(connect(sockfd, (sockaddr *)&serv_addr, sizeof(serv_addr)) == -1, "socket connect error");
    while (true)
    {
        char buf[1024];
        bzero(&buf, sizeof(buf));
        scanf("%s", buf);
        // 从数据
        ssize_t write_bytes = write(sockfd, buf, sizeof(buf));
        if (write_bytes == -1) // 发送数据发生错误
        {
            printf("socket already disconnected, can't write any more!\n");
            break;
        }
        bzero(&buf, sizeof(buf));
        ssize_t read_bytes = read(sockfd, buf, sizeof(buf)); // 从服务器socket读取到缓冲区，返回已读数据大小
        if (read_bytes > 0)
        {
            printf("message from server: %s\n", buf);
        }
        else if (read_bytes == 0) // read返回0表示服务器断开连接，可以等下再连接
        {
            printf("server socket disconnected!\n");
            break;
        }
        else if (read_bytes == -1) // 返回-1表示发生错误
        {
            close(sockfd);
            errif(true, "socket read error");
        }
    }
    return 0;
}