#include "util.h"
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
void errif(bool condition, const char *errmsg)
{
    if (condition)
    {
        perror(errmsg);
        exit(EXIT_FAILURE);
    }
}

void setnonblocking(int fd)
{
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}