// BaseSocket class - inherit & implement to make UNIX/INET domain sockets

#ifndef __HPP_BASESOCKET
#define __HPP_BASESOCKET


// INCLUDES

#include "platform.h"

#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <exception>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef __GCCVER3
using namespace std;
#endif

int selectEINTR(int numfds, fd_set * readfds, fd_set * writefds, fd_set * exceptfds, struct timeval *timeout, bool honour_reloadconfig = false);

class BaseSocket
{
public:
	// create socket from FD - must be overridden to clear the relevant address structs in derived classes
	BaseSocket(int fd);

	// make a socket a listening server socket
	int listen(int queue);

	// grab socket's FD, e.g. for passing to selectEINTR
	// use sparingly, and DO NOT do manual data transfer with it
	int getFD();
	
	// close socket
	void close();
	
	// set socket-wide timeout (is this actually used? all methods accept their own individual timeouts)
	void setTimeout(int t);
	// get timeout (is this actually used?)
	int getTimeout();

	// close & reset the connection - these must clear address structures & call baseReset/baseAccept
	virtual void reset()=0;
	virtual BaseSocket* accept()=0;

	// non-blocking check for input data
	bool checkForInput();
	// blocking check for data, can be told to break on signal triggered config reloads (-r)
	void checkForInput(int timeout, bool honour_reloadconfig = false) throw(exception);
	// non-blocking check for writable socket
	bool readyForOutput();
	// blocking check, can break on config reloads
	void readyForOutput(int timeout, bool honour_reloadconfig = false) throw(exception);
	
	// get a line from the socket - can break on config reloads
	int getLine(char *buff, int size, int timeout, bool honour_reloadconfig = false) throw(exception);
	
	// write buffer to string - throws exception on error
	void writeString(const char *line) throw(exception);
	// write buffer to string - can be told not to do an initial readyForOutput, and told to break on -r
	bool writeToSocket(char *buff, int len, unsigned int flags, int timeout, bool check_first = true, bool honour_reloadconfig = false);
	// read from socket, returning number of bytes read
	int readFromSocketn(char *buff, int len, unsigned int flags, int timeout);
	// read from socket, returning error status - can be told to skip initial checkForInput, and to break on -r
	int readFromSocket(char *buff, int len, unsigned int flags, int timeout, bool check_first = true, bool honour_reloadconfig = false);
	// write to socket, throwing exception on error - can be told to break on -r
	void writeToSockete(char *buff, int len, unsigned int flags, int timeout, bool honour_reloadconfig = false) throw(exception);

protected:
	// socket-wide timeout (is this actually used?)
	int timeout;
	// length of address of other end of socket (e.g. size of sockaddr_in or sockaddr_un)
	socklen_t peer_adr_length;
	// socket FD
	int sck;

	// constructor - sets default values. override this if you actually wish to create a default socket.
	BaseSocket();
	// destructor - closes socket
	~BaseSocket();
	
	// performs accept(). call from derived classes' accept method
	int baseAccept(struct sockaddr *acc_adr, socklen_t *acc_adr_length);
	// closes socket & resets timeout to default - call from derived classes' reset method
	void baseReset();
};

#endif