#include "Buffer.h"
#include <iostream>
Buffer::Buffer() {}

Buffer::~Buffer() {}

void Buffer::append(const char *str, int size) {
  for (int i = 0; i < size; ++i) {
    if (str[i] == '\0')
      break;
    _buf.push_back(str[i]);
  }
}
ssize_t Buffer::size() { return _buf.size(); }
const char *Buffer::c_str() { return _buf.c_str(); }
void Buffer::clear() { _buf.clear(); }

void Buffer::setBuf(const char *str) {
  _buf.clear();
  _buf.append(str);
}

void Buffer::getline() {
  _buf.clear();
  std::getline(std::cin, _buf);
}