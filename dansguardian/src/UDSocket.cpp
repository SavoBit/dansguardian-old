//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@jadeb// .com).
//For support go to http://groups.yahoo.com/group/dansguardian

//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include "platform.h"
#include <syslog.h>
#include "UDSocket.hpp"
#include <csignal>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <cerrno>
#include <unistd.h>
#include <iostream>
#include <stdexcept>



// This class contains client and server Internet socket init and handling
// code as well as functions for testing and working with the socket FDs.


UDSocket::UDSocket() :timeout(5), isclosed(false), isused(false) {
    sck_un = socket(PF_UNIX, SOCK_STREAM, 0);
    memset(&my_adr_un, 0, sizeof my_adr_un);
    memset(&peer_adr_un, 0, sizeof peer_adr_un);
    my_adr_un.sun_family = AF_UNIX;
    peer_adr_un.sun_family = AF_UNIX;

}

UDSocket::~UDSocket() {  // closing of fd is only automatic if socket not used
    if (!isused && !isclosed) {
        ::close(sck_un);
    }
}

// For creating a socket from the 3 things that make a socket
UDSocket::UDSocket(int newfd, struct sockaddr_un myadr) :timeout(5), isclosed(false), isused(true) {
    sck_un = newfd;
    my_adr_un = myadr;
    my_adr_un_length = sizeof(my_adr_un.sun_family) + strlen(my_adr_un.sun_path);
}

void UDSocket::reset() {
    ::close(sck_un);
    timeout = 5;
    isclosed = false;
    isused = false;
    sck_un = socket(PF_UNIX, SOCK_STREAM, 0);
    memset(&my_adr_un, 0, sizeof my_adr_un);
    memset(&peer_adr_un, 0, sizeof peer_adr_un);
    my_adr_un.sun_family = AF_UNIX;
    peer_adr_un.sun_family = AF_UNIX;
}

int UDSocket::connect(const char* path) {  // to make a connection to a serv
    #ifdef DGDEBUG
        std::cout << "uds connect:" << path << std::endl;
    #endif
    isused = true;
    strcpy(my_adr_un.sun_path, path);

    // Note to fix.  This could probably be replaced with a test for
    // struct sockaddr_un.sun_len in configure.ac
    // Daniel Barron - Sun 30th January 2005

    #ifdef __BSD
        #ifdef SCM_RIGHTS     /* 4.3BSD Reno and later */
            my_adr_un_length = sizeof(my_adr_un.sun_len) + sizeof(my_adr_un.sun_family) + strlen(my_adr_un.sun_path) + 1;
            my_adr_un.sun_len = my_adr_un_length ;
        #endif
    #else
        my_adr_un_length = sizeof(my_adr_un.sun_family) + strlen(my_adr_un.sun_path);
    #endif
    return ::connect(sck_un, (struct sockaddr *) &my_adr_un, my_adr_un_length);
}


int UDSocket::bind(const char* path) {  // to bind a unix domain socket to a path
    isused = true;
    unlink(path);
    strcpy(my_adr_un.sun_path, path);

    // Note to fix.  This could probably be replaced with a test for
    // struct sockaddr_un.sun_len in configure.ac
    // Daniel Barron - Sun 30th January 2005

    #ifdef __BSD
        #ifdef SCM_RIGHTS     /* 4.3BSD Reno and later */
            my_adr_un_length = sizeof(my_adr_un.sun_len) + sizeof(my_adr_un.sun_family) + strlen(my_adr_un.sun_path) + 1;
            my_adr_un.sun_len = my_adr_un_length ;
        #endif
    #else
        my_adr_un_length = sizeof(my_adr_un.sun_family) + strlen(my_adr_un.sun_path);
    #endif
    return ::bind(sck_un, (struct sockaddr *) &my_adr_un, my_adr_un_length);
}

int UDSocket::listen(int queue) {  // mark a socket as a listening server sockt
    return ::listen(sck_un, queue);
}


UDSocket UDSocket::accept() {  // receive the incomming connection
    my_adr_un_length = sizeof(my_adr_un.sun_family) + strlen(my_adr_un.sun_path);
    // OS X defines accept as:
    // int accept(int s, struct sockaddr *addr, int *addrlen);
    // but everyone else as:
    // int accept(int s, struct sockaddr *addr, socklen_t *addrlen);
#ifdef __DARWIN
    int newfd = ::accept(sck_un, (struct sockaddr *) &my_adr_un, (int *)&my_adr_un_length);
#else
    int newfd = ::accept(sck_un, (struct sockaddr *) &my_adr_un, &my_adr_un_length);
#endif
    UDSocket s(newfd, my_adr_un);
    return s;
}


int UDSocket::getFD() {
    return sck_un;
}

void UDSocket::setFD(int newfd) {
    sck_un = newfd;
}

void UDSocket::close() {  // close the socket (fd)
    ::close(sck_un);
    isclosed = true;
}

void UDSocket::setTimeout(int t) {  // set the socket-wide timeout
    timeout = t;
}

int UDSocket::getTimeout() {
    return timeout;
}


bool UDSocket::checkForInput() {  // non-blocking check
    fd_set fdSet;
    FD_ZERO(&fdSet);  // clear the set
    FD_SET(sck_un, &fdSet);  // add fd to the set
    timeval t;  // timeval struct
    t.tv_sec = 0;
    t.tv_usec = 0;
    if (UDSocket::selectEINTR(sck_un + 1, &fdSet, NULL, NULL, &t) < 1) {
        return false;
    }
    return true;
}


void UDSocket::checkForInput(int timeout) throw (exception)  {
                                            // blocks if socket blocking
                                            // until timeout
    fd_set fdSet;
    FD_ZERO(&fdSet);  // clear the set
    FD_SET(sck_un, &fdSet);  // add fd to the set
    timeval t;  // timeval struct
    t.tv_sec = timeout;
    t.tv_usec = 0;
    if (UDSocket::selectEINTR(sck_un + 1, &fdSet, NULL, NULL, &t) < 1) {
        string err("select() on input (unix socket): ");
        throw runtime_error(err + (errno? strerror(errno) : "timeout"));
    }
}


bool UDSocket::readyForOutput() {  // non-blocking check
    fd_set fdSet;
    FD_ZERO(&fdSet);  // clear the set
    FD_SET(sck_un, &fdSet);  // add fd to the set
    timeval t;  // timeval struct
    t.tv_sec = 0;
    t.tv_usec = 0;
    if (UDSocket::selectEINTR(sck_un + 1, NULL, &fdSet, NULL, &t) < 1) {
        return false;
    }
    return true;
}


void UDSocket::readyForOutput(int timeout) throw(exception) {
                                            // blocks if socket blocking
                                            // until timeout
    fd_set fdSet;
    FD_ZERO(&fdSet);  // clear the set
    FD_SET(sck_un, &fdSet);  // add fd to the set
    timeval t;  // timeval struct
    t.tv_sec = timeout;
    t.tv_usec = 0;
    if (UDSocket::selectEINTR(sck_un + 1, NULL, &fdSet, NULL, &t) < 1) {
        string err("select() on output (unix socket): ");
        throw runtime_error(err + (errno? strerror(errno) : "timeout"));
    }
}

int UDSocket::getline(char* buff, int size, int timeout) throw(exception) {

    char b[1];
    int rc;
    int i = 0;
    b[0] = 0;
    while(i < (size - 1)) {
        rc = readFromSocket(b, 1, 0, timeout);
        if (rc < 0) {
            throw runtime_error(string("Can't read from unix socket: ") + strerror(errno));  // on error
        }
        if (rc == 0) {  // eof, other end closed so
            b[0] = '\n'; // force it to return what read
        }
        buff[i] = b[0];
        i++;
        if (b[0] == '\n') {
            buff[i - 1] = '\0';
            break;
        }
    }
    buff[i] = '\0';
    return i;
}


void UDSocket::writeString(const char* line) throw(exception) {
    int l = strlen(line);
    if (!writeToSocket((char*)line, l, 0, 10)) {
        throw runtime_error(string("Can't write to unix socket: ") + strerror(errno));
    }
}

void UDSocket::writeToSockete(char* buff, int len, unsigned int flags, int timeout) throw(exception) {
  if (!UDSocket::writeToSocket(buff, len, flags, timeout)) {
      throw runtime_error(string("Can't write to unix socket: ") + strerror(errno));
  }
}

bool UDSocket::writeToSocket(char* buff, int len, unsigned int flags, int timeout) {
    int actuallysent = 0;
    int sent;
    while (actuallysent < len) {
        try {
            readyForOutput(timeout);  // throws exception on error or timeout
        } catch (exception& e) {
            return false;
        }
        sent = send(sck_un, buff + actuallysent, len - actuallysent, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;  // was interupted by signal so restart
            }
            return false;
        }
        if (sent == 0) {
            return false; // other end is closed
        }
        actuallysent += sent;
    }
    return true;
}

// read a specified expected amount and return what actually read
int UDSocket::readFromSocketn(char* buff, int len, unsigned int flags, int timeout) {
    int cnt, rc;
    cnt = len;
    while (cnt > 0) {
        try {
            checkForInput(timeout);  // throws exception on error or timeout
        } catch (exception& e) {
            return -1;
        }
        rc = recv(sck_un, buff, len, flags);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (rc == 0) { // eof
            return len - cnt;
        }
        buff += rc;
        cnt -= rc;
    }
    return len;
}

// read what's available and return how much read
int UDSocket::readFromSocket(char* buff, int len, unsigned int flags, int timeout) {
    int rc;
    try {
        checkForInput(timeout);
    } catch (exception& e) {
        return -1;
    }
    while (true) {
        rc = recv(sck_un, buff, len, flags);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
        }
        break;
    }
    return rc;
}

// a wrapper for select so that it auto-restarts after an ENINTR
int UDSocket::selectEINTR(int numfds, fd_set * readfds, fd_set * writefds, fd_set * exceptfds, struct timeval * timeout) {
    int rc;
    while (true) {  // using the while as a restart point with continue
        rc = select(numfds, readfds, writefds, exceptfds, timeout);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;  // was interupted by a signal so restart
            }
        }
        break;  // end the while
    }
    return rc;  // return status
}
