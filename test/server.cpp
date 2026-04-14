#include "Server.h"
#include "EventLoop.h"
#include "Logger.h"
int main()
{
    // start传入文件名，文件写入文件。否则输出屏幕
    Logger::getInstance().start();
    LOG_INFO("Server start...");
    EventLoop *loop = new EventLoop();
    Server *server = new Server(loop);
    loop->loop();
    Logger::getInstance().stop();
    return 0;
}
