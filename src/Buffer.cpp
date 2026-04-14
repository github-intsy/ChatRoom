#include "Buffer.h"
#include <iostream>
Buffer::Buffer() {}

Buffer::~Buffer() {}

void Buffer::append(const char *str, int size)
{
    if (str && size > 0)
    {
        _buf.append(str, size); // 支持二进制数据
    }
}
ssize_t Buffer::size() { return _buf.size(); }
const char *Buffer::c_str() { return _buf.c_str(); }
void Buffer::clear() { _buf.clear(); }

void Buffer::setBuf(const char *str, size_t size)
{
    _buf.clear();
    if(str && size > 0)
        _buf.append(str, size);
}

void Buffer::getline()
{
    _buf.clear();
    std::getline(std::cin, _buf);
}

std::string Buffer::getBuffer()
{
    return _buf;
}

bool Buffer::empty()
{
    return _buf.empty();
}

void Buffer::eraseFront(size_t len)
{
    if(len >= _buf.size())
        _buf.clear();
    else
        _buf.erase(0, len);
}