// Wrappers around some useful FD functions
// Please use *only* for files, not sockets!

#ifndef __HPP_FDFUNCS
#define __HPP_FDFUNCS

#include "platform.h"

#include <unistd.h>
#include <cerrno>

// wrappers around FD read/write that restart on EINTR
int readEINTR(int fd, char *buf, unsigned int count);
int writeEINTR(int fd, char *buf, unsigned int count);

#endif
