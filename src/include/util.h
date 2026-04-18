#ifndef UTIL_H
#define UTIL_H
#include <string>
std::string encodePacket(const std::string &body);
inline bool isBase64(unsigned char c);
std::string base64Decode(const std::string &encoded_string);
#endif