// Base socket class - inherit this to implement UNIX/INET domain sockets

//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.

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

// INCLUDES

#include "BaseSocket.hpp"

#include <csignal>
#include <fcntl.h>
#include <sys/time.h>
#include <pwd.h>
#include <cerrno>
#include <unistd.h>
#include <stdexcept>
#include <syslog.h>

// GLOBALS
extern bool reloadconfig;


// IMPLEMENTATION

// a wrapper for select so that it auto-restarts after an EINTR.
// can be instructed to watch out for signal triggered config reloads.
int selectEINTR(int numfds, fd_set * readfds, fd_set * writefds, fd_set * exceptfds, struct timeval *timeout, bool honour_reloadconfig)
{
	int rc;
	errno = 0;
	while (true) {		// using the while as a restart point with continue
		rc = select(numfds, readfds, writefds, exceptfds, timeout);
		if (rc < 0) {
			if (errno == EINTR && (honour_reloadconfig? !reloadconfig : true)) {
				continue;  // was interupted by a signal so restart
			}
		}
		break;  // end the while
	}
	return rc;  // return status
}

// This class contains client and server socket init and handling
// code as well as functions for testing and working with the socket FDs.

// constructor - override this if desired to create an actual socket at startup
BaseSocket::BaseSocket():timeout(5), sck(-1)
{}

// create socket from FD - must be overridden to clear the relevant address structs
BaseSocket::BaseSocket(int fd):timeout(5)
{
	sck = fd;
}

// destructor - close socket
BaseSocket::~BaseSocket()
{
	// close fd if socket not used
	if (sck > -1) {
		::close(sck);
	}
}

// reset - close socket & reset timeout.
// call this in derived classes' reset() method, which should also clear address structs
void BaseSocket::baseReset()
{
	if (sck > -1) {
		::close(sck);
		sck = -1;
	}
	timeout = 5;
}

// mark a socket as a listening server socket
int BaseSocket::listen(int queue)
{
	return ::listen(sck, queue);
}

// receive an incoming connection & return FD
// call this in accept methods of derived classes, which should pass in empty sockaddr & socklen_t to be filled out
int BaseSocket::baseAccept(struct sockaddr *acc_adr, socklen_t *acc_adr_length)
{	

	// OS X defines accept as:
	// int accept(int s, struct sockaddr *addr, int *addrlen);
	// but everyone else as:
	// int accept(int s, struct sockaddr *addr, socklen_t *addrlen);
#ifdef __DARWIN
	int newfd =::accept(sck, acc_adr, (int *) acc_adr_length);
#else
	int newfd =::accept(sck, acc_adr, acc_adr_length);
#endif
	return newfd;
}

// return socket's FD - please use sparingly and DO NOT do manual data transfer using it
int BaseSocket::getFD()
{
	return sck;
}

// close the socket
void BaseSocket::close()
{
	if (sck > -1) {
		::close(sck);
		sck = -1;
	}
}

// set the socket-wide timeout
void BaseSocket::setTimeout(int t)
{
	timeout = t;
}

// return timeout
int BaseSocket::getTimeout()
{
	return timeout;
}

// non-blocking check to see if there is data waiting on socket
bool BaseSocket::checkForInput()
{
	fd_set fdSet;
	FD_ZERO(&fdSet);  // clear the set
	FD_SET(sck, &fdSet);  // add fd to the set
	timeval t;  // timeval struct
	t.tv_sec = 0;
	t.tv_usec = 0;
	if (selectEINTR(sck + 1, &fdSet, NULL, NULL, &t) < 1) {
		return false;
	}
	return true;
}

// blocking check for waiting data - blocks for up to given timeout, can be told to break on signal-triggered config reloads
void BaseSocket::checkForInput(int timeout, bool honour_reloadconfig) throw(exception)
{
	// blocks if socket blocking
	// until timeout
	fd_set fdSet;
	FD_ZERO(&fdSet);  // clear the set
	FD_SET(sck, &fdSet);  // add fd to the set
	timeval t;  // timeval struct
	t.tv_sec = timeout;
	t.tv_usec = 0;
	if (selectEINTR(sck + 1, &fdSet, NULL, NULL, &t, honour_reloadconfig) < 1) {
		string err("select() on input: ");
		throw runtime_error(err + (errno ? strerror(errno) : "timeout"));
	}
}

// non-blocking check to see if a socket is ready to be written
bool BaseSocket::readyForOutput()
{
	fd_set fdSet;
	FD_ZERO(&fdSet);  // clear the set
	FD_SET(sck, &fdSet);  // add fd to the set
	timeval t;  // timeval struct
	t.tv_sec = 0;
	t.tv_usec = 0;
	if (selectEINTR(sck + 1, NULL, &fdSet, NULL, &t) < 1) {
		return false;
	}
	return true;
}

// blocking equivalent of above, can be told to break on signal-triggered reloads
void BaseSocket::readyForOutput(int timeout, bool honour_reloadconfig) throw(exception)
{
	// blocks if socket blocking
	// until timeout
	fd_set fdSet;
	FD_ZERO(&fdSet);  // clear the set
	FD_SET(sck, &fdSet);  // add fd to the set
	timeval t;  // timeval struct
	t.tv_sec = timeout;
	t.tv_usec = 0;
	if (selectEINTR(sck + 1, NULL, &fdSet, NULL, &t, honour_reloadconfig) < 1) {
		string err("select() on output: ");
		throw runtime_error(err + (errno ? strerror(errno) : "timeout"));
	}
}

// read a line from the socket, can be told to break on config reloads
int BaseSocket::getLine(char *buff, int size, int timeout, bool honour_reloadconfig) throw(exception)
{
	char b[1];
	int rc;
	int i = 0;
	while (i < (size - 1)) {
		rc = readFromSocket(b, 1, 0, timeout, true, honour_reloadconfig);
		if (rc < 0) {
			throw runtime_error(string("Can't read from socket: ") + strerror(errno));  // on error
		}
		//if socket closed or newline received...
		if ((rc == 0) || (b[0] == '\n')) {
			buff[i] = '\0';  // ...terminate string & return what read
			return i;
		}
		buff[i] = b[0];
		i++;
	}
	// oh dear - buffer end reached before we found a newline
	buff[i] = '\0';
	return i;
}

// write line to socket
void BaseSocket::writeString(const char *line) throw(exception)
{
	int l = strlen(line);
	if (!writeToSocket((char *) line, l, 0, timeout)) {
		throw runtime_error(string("Can't write to socket: ") + strerror(errno));
	}
}

// write data to socket - throws exception on failure, can be told to break on config reloads
void BaseSocket::writeToSockete(char *buff, int len, unsigned int flags, int timeout, bool honour_reloadconfig) throw(exception)
{
	if (!writeToSocket(buff, len, flags, timeout, honour_reloadconfig)) {
		throw runtime_error(string("Can't write to socket: ") + strerror(errno));
	}
}

// write data to socket - can be told not to do an initial readyForOutput, and to break on config reloads
bool BaseSocket::writeToSocket(char *buff, int len, unsigned int flags, int timeout, bool check_first, bool honour_reloadconfig)
{
	int actuallysent = 0;
	int sent;
	while (actuallysent < len) {
		if (check_first) {
			try {
				readyForOutput(timeout, honour_reloadconfig);  // throws exception on error or timeout
			}
			catch(exception & e) {
				return false;
			}
		}
		sent = send(sck, buff + actuallysent, len - actuallysent, 0);
		if (sent < 0) {
			if (errno == EINTR && (honour_reloadconfig ? !reloadconfig : true)) {
				continue;  // was interupted by signal so restart
			}
			return false;
		}
		if (sent == 0) {
			return false;  // other end is closed
		}
		actuallysent += sent;
	}
	return true;
}

// read a specified expected amount and return what actually read
int BaseSocket::readFromSocketn(char *buff, int len, unsigned int flags, int timeout)
{
	int cnt, rc;
	cnt = len;
	while (cnt > 0) {
		try {
			checkForInput(timeout);  // throws exception on error or timeout
		}
		catch(exception & e) {
			return -1;
		}
		rc = recv(sck, buff, len, flags);
		if (rc < 0) {
			if (errno == EINTR) {
				continue;
			}
			return -1;
		}
		if (rc == 0) {	// eof
			return len - cnt;
		}
		buff += rc;
		cnt -= rc;
	}
	return len;
}

// read what's available and return error status - can be told not to do an initial checkForInput, and to break on reloads
int BaseSocket::readFromSocket(char *buff, int len, unsigned int flags, int timeout, bool check_first, bool honour_reloadconfig)
{
	int rc;
	if (check_first) {
		try {
			checkForInput(timeout, honour_reloadconfig);
		} catch(exception & e) {
			return -1;
		}
	}
	while (true) {
		rc = recv(sck, buff, len, flags);
		if (rc < 0) {
			if (errno == EINTR && (honour_reloadconfig ? !reloadconfig : true)) {
				continue;
			}
		}
		break;
	}
	return rc;
}
