#include "util.h"
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <arpa/inet.h>
#include <filesystem>
std::string encodePacket(const std::string &body)
{
  uint32_t len = htonl(static_cast<uint32_t>(body.size()));
  std::string full;
  full.append(reinterpret_cast<const char *>(&len), 4);
  full.append(body);
  return full;
}
inline bool isBase64(unsigned char c)
{
  return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64Decode(const std::string &encoded_string)
{
  static const std::string base64_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789+/";

  int in_len = static_cast<int>(encoded_string.size());
  int i = 0;
  int in_ = 0;
  unsigned char char_array_4[4], char_array_3[3];
  std::string ret;

  while (in_len-- && (encoded_string[in_] != '=') && isBase64(encoded_string[in_]))
  {
    char_array_4[i++] = encoded_string[in_];
    in_++;
    if (i == 4)
    {
      for (i = 0; i < 4; i++)
        char_array_4[i] = static_cast<unsigned char>(base64_chars.find(char_array_4[i]));

      char_array_3[0] = static_cast<unsigned char>((char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4));
      char_array_3[1] = static_cast<unsigned char>(((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2));
      char_array_3[2] = static_cast<unsigned char>(((char_array_4[2] & 0x3) << 6) + char_array_4[3]);

      for (i = 0; i < 3; i++)
        ret += static_cast<char>(char_array_3[i]);
      i = 0;
    }
  }

  if (i)
  {
    for (int j = i; j < 4; j++)
      char_array_4[j] = 0;

    for (int j = 0; j < 4; j++)
      char_array_4[j] = static_cast<unsigned char>(base64_chars.find(char_array_4[j]));

    char_array_3[0] = static_cast<unsigned char>((char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4));
    char_array_3[1] = static_cast<unsigned char>(((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2));
    char_array_3[2] = static_cast<unsigned char>(((char_array_4[2] & 0x3) << 6) + char_array_4[3]);

    for (int j = 0; j < i - 1; j++)
      ret += static_cast<char>(char_array_3[j]);
  }

  return ret;
}
