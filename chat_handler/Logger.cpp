#include "Logger.h"
#include <iostream>
#include <ctime>

Logger &Logger::getInstance()
{
    static Logger instance;
    return instance;
}

Logger::Logger() : _running(false), _os(nullptr)
{
}

Logger::~Logger()
{
    stop();
}

void Logger::start(const std::string &filename)
{
    if (filename.empty())
    {
        _os = &std::cout;
    }
    else
    {
        _ofs.open(filename, std::ios::out | std::ios::app);
        if (_ofs.is_open())
            _os = &_ofs;
        else
            _os = &std::cout;
    }
    _running = true;
    _writeThread = std::thread(&Logger::writeThreadFunc, this);
}

void Logger::stop()
{
    _running = false;
    _cv.notify_all();
    if (_writeThread.joinable())
        _writeThread.join();
    if (_ofs.is_open())
        _ofs.close();
}

void Logger::log(LogLevel level, const std::string &msg)
{
    std::string levelStr;
    switch (level)
    {
    case DEBUG:
        levelStr = "[DEBUG]";
        break;
    case INFO:
        levelStr = "[INFO]";
        break;
    case WARN:
        levelStr = "[WARN]";
        break;
    case ERROR:
        levelStr = "[ERROR]";
        break;
    }

    // 时间戳
    time_t now = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S]", localtime(&now));

    std::string logMsg = std::string(buf) + " " + levelStr + " " + msg;

    {
        std::lock_guard<std::mutex> lock(_mutex);
        _logQueue.push(logMsg);
    }

    _cv.notify_one();
}

void Logger::writeThreadFunc()
{
    while (_running)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [this]()
                 { return !_logQueue.empty() || !_running; });

        while (!_logQueue.empty())
        {
            std::string msg = _logQueue.front();
            _logQueue.pop();

            *_os << msg << std::endl;
        }
    }
}