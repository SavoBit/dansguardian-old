//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@jadeb//.com).
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
#include "Socket.hpp"
#include <csignal>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <cerrno>
#include <unistd.h>
#include <string>
#include <stdexcept>


// This class contains client and server Internet socket init and handling
// code as well as functions for testing and working with the socket FDs.


Socket::Socket() :timeout(5), isclosed(false), isused(false) {
    sck_inet = socket(AF_INET, SOCK_STREAM, 0);
    memset(&my_adr_inet, 0, sizeof my_adr_inet);
    memset(&peer_adr_inet, 0, sizeof peer_adr_inet);
    my_adr_inet.sin_family = AF_INET;
    peer_adr_inet.sin_family = AF_INET;
    peer_adr_inet_length = sizeof(struct sockaddr_in);
}

Socket::~Socket() {  // closing of fd is only automatic if socket not used
    if (!isused && !isclosed) {
        ::close(sck_inet);
    }
}

// For creating a socket from the 3 things that make a socket
Socket::Socket(int newfd, struct sockaddr_in myip, struct sockaddr_in peerip) :timeout(5), isclosed(false), isused(true) {
    sck_inet = newfd;
    memset(&my_adr_inet, 0, sizeof my_adr_inet);  // ***
    memset(&peer_adr_inet, 0, sizeof peer_adr_inet);  // ***
    my_adr_inet.sin_family = AF_INET;  // *** Fix suggested by
    peer_adr_inet.sin_family = AF_INET;  // *** Christopher Weimann
    my_adr_inet = myip;
    peer_adr_inet = peerip;
    peer_adr_inet_length = sizeof(struct sockaddr_in);
}

void Socket::reset() {
    ::close(sck_inet);
    timeout = 5;
    isclosed = false;
    isused = false;
    sck_inet = socket(AF_INET, SOCK_STREAM, 0);
    memset(&my_adr_inet, 0, sizeof my_adr_inet);
    memset(&peer_adr_inet, 0, sizeof peer_adr_inet);
    my_adr_inet.sin_family = AF_INET;
    peer_adr_inet.sin_family = AF_INET;
    peer_adr_inet_length = sizeof(struct sockaddr_in);
}

int Socket::connect(std::string ip, int port) {  // to make a connection to a serv
    isused = true;
    int len_inet = sizeof my_adr_inet;
    peer_adr_inet.sin_port = htons(port);
    inet_aton(ip.c_str(), &peer_adr_inet.sin_addr);
    return ::connect(sck_inet, (struct sockaddr *) &peer_adr_inet, len_inet);
}


int Socket::bind(int port) {  // to bind a socket to a port
    isused = true;
    int len_inet = sizeof my_adr_inet;
    int i = 1;
    setsockopt(sck_inet, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));
    my_adr_inet.sin_port = htons(port);
    return ::bind(sck_inet, (struct sockaddr *) &my_adr_inet, len_inet);
}

int Socket::bind(std::string ip, int port) {  // to bind a socket to port,ip
                                         // for hetrogenious machines
    isused = true;
    int len_inet = sizeof my_adr_inet;
    int i = 1;
    setsockopt(sck_inet, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));
    my_adr_inet.sin_port = htons(port);
    my_adr_inet.sin_addr.s_addr = inet_addr(ip.c_str());
    return ::bind(sck_inet, (struct sockaddr *) &my_adr_inet, len_inet);
}

int Socket::listen(int queue) {  // mark a socket as a listening server sockt
    return ::listen(sck_inet, queue);
}

Socket Socket::accept() {  // receive the incomming connection
    peer_adr_inet_length = sizeof(struct sockaddr_in);
    // OS X defines accept as:
    // int accept(int s, struct sockaddr *addr, int *addrlen);
    // but everyone else as:
    // int accept(int s, struct sockaddr *addr, socklen_t *addrlen);
#ifdef __DARWIN
    int newfd = ::accept(sck_inet, (struct sockaddr *) &peer_adr_inet, (int *)&peer_adr_inet_length);
#else
    int newfd = ::accept(sck_inet, (struct sockaddr *) &peer_adr_inet, &peer_adr_inet_length);
#endif
    Socket s(newfd, my_adr_inet, peer_adr_inet);
    return s;
}

std::string Socket::getPeerIP() {  // find the ip of the client connecting to us
    return inet_ntoa(peer_adr_inet.sin_addr);
}

int Socket::getPeerSourcePort() {  // find the port of the client connecting to us
    return ntohs(peer_adr_inet.sin_port);
}

int Socket::getFD() {
    return sck_inet;
}

void Socket::setFD(int newfd) {
    sck_inet = newfd;
}

void Socket::close() {  // close the socket (fd)
    ::close(sck_inet);
    isclosed = true;
}

void Socket::setTimeout(int t) {  // set the socket-wide timeout
    timeout = t;
}

int Socket::getTimeout() {
    return timeout;
}


bool Socket::checkForInput() {  // non-blocking check
    fd_set fdSet;
    FD_ZERO(&fdSet);  // clear the set
    FD_SET(sck_inet, &fdSet);  // add fd to the set
    timeval t;  // timeval struct
    t.tv_sec = 0;
    t.tv_usec = 0;
    if (Socket::selectEINTR(sck_inet + 1, &fdSet, NULL, NULL, &t) < 1) {
        return false;
    }
    return true;
}


void Socket::checkForInput(int timeout) throw (exception)  {
                                            // blocks if socket blocking
                                            // until timeout
    fd_set fdSet;
    FD_ZERO(&fdSet);  // clear the set
    FD_SET(sck_inet, &fdSet);  // add fd to the set
    timeval t;  // timeval struct
    t.tv_sec = timeout;
    t.tv_usec = 0;
    if (Socket::selectEINTR(sck_inet + 1, &fdSet, NULL, NULL, &t) < 1) {
        string err("select() on input: ");
        throw runtime_error(err + (errno? strerror(errno) : "timeout"));
    }
}


bool Socket::readyForOutput() {  // non-blocking check
    fd_set fdSet;
    FD_ZERO(&fdSet);  // clear the set
    FD_SET(sck_inet, &fdSet);  // add fd to the set
    timeval t;  // timeval struct
    t.tv_sec = 0;
    t.tv_usec = 0;
    if (Socket::selectEINTR(sck_inet + 1, NULL, &fdSet, NULL, &t) < 1) {
        return false;
    }
    return true;
}


void Socket::readyForOutput(int timeout) throw(exception) {
                                            // blocks if socket blocking
                                            // until timeout
    fd_set fdSet;
    FD_ZERO(&fdSet);  // clear the set
    FD_SET(sck_inet, &fdSet);  // add fd to the set
    timeval t;  // timeval struct
    t.tv_sec = timeout;
    t.tv_usec = 0;
    if (Socket::selectEINTR(sck_inet + 1, NULL, &fdSet, NULL, &t) < 1) {
        string err("select() on output: ");
        throw runtime_error(err + (errno? strerror(errno) : "timeout"));
    }
}

int Socket::getline(char* buff, int size, int timeout) throw(exception) {

    char b[1];
    int rc;
    int i = 0;
    while(i < (size-1)) {
        rc = readFromSocket(b, 1, 0, timeout);
        if (rc < 0) {
            throw runtime_error(string("Can't read from socket: ") + strerror(errno));  // on error
        }
        //if socket closed or newline received...
        if ((rc == 0)||(b[0]=='\n')) {
            buff[i] = '\0'; // ...terminate string & return what read
            return i;
        }
        buff[i]=b[0];
        i++;
    }
    // oh dear - buffer end reached before we found a newline
    buff[i]='\0';
    return i;
}


void Socket::writeString(const char* line) throw(exception) {
    int l = strlen(line);
    if (!writeToSocket((char*)line, l, 0, timeout)) {
        throw runtime_error(string("Can't write to socket: ") + strerror(errno));
    }
}

void Socket::writeToSockete(char* buff, int len, unsigned int flags, int timeout) throw(exception) {
  if (!Socket::writeToSocket(buff, len, flags, timeout)) {
      throw runtime_error(string("Can't write to socket: ") + strerror(errno));
  }
}

bool Socket::writeToSocket(char* buff, int len, unsigned int flags, int timeout) {
    int actuallysent = 0;
    int sent;
    while (actuallysent < len) {
        try {
            readyForOutput(timeout);  // throws exception on error or timeout
        } catch (exception& e) {
            return false;
        }
        sent = send(sck_inet, buff + actuallysent, len - actuallysent, 0);
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
int Socket::readFromSocketn(char* buff, int len, unsigned int flags, int timeout) {
    int cnt, rc;
    cnt = len;
    while (cnt > 0) {
        try {
            checkForInput(timeout);  // throws exception on error or timeout
        } catch (exception& e) {
            return -1;
        }
        rc = recv(sck_inet, buff, len, flags);
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
int Socket::readFromSocket(char* buff, int len, unsigned int flags, int timeout) {
    int rc;
    try {
        checkForInput(timeout);
    } catch (exception& e) {
        return -1;
    }
    while (true) {
        rc = recv(sck_inet, buff, len, flags);
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
int Socket::selectEINTR(int numfds, fd_set * readfds, fd_set * writefds, fd_set * exceptfds, struct timeval * timeout) {
    int rc;
    errno=0;
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
